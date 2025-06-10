#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#define DEFAULT_PORT 8080
#define MONITOR_PORT 8081
#define BUFFER_SIZE 1024
#define LARGE_BUFFER_SIZE 8192  // Для длинных сообщений монитору
#define REQUEST_TASK "GET_TASK"
#define SUBMIT_RESULT "SUBMIT_RESULT"
#define NO_MORE_TASKS "NO_TASKS"
#define TASK_RECEIVED "TASK_RECEIVED"

// Константы для протокола монитора
#define MONITOR_REGISTER "REGISTER_MONITOR"
#define MONITOR_ACK "MONITOR_ACK"
#define SERVER_EVENT "SERVER_EVENT"
#define CLIENT_EVENT "CLIENT_EVENT"
#define SYSTEM_EVENT "SYSTEM_EVENT"
#define MONITOR_SHUTDOWN "MONITOR_SHUTDOWN"

// Глобальные переменные для обработки сигналов
volatile sig_atomic_t server_running = 1;
int sockfd_global = -1;

// Структура для хранения задач
typedef struct {
    char **tasks;
    int total_tasks;
    int current_task;
    int task_size;
    char *original_text;
} TaskPortfolio;

// Структура для хранения результатов шифрования
typedef struct {
    char *encrypted_text;
    int position;
    int received;
} EncryptedFragment;

// Структура для хранения информации о клиентах
typedef struct {
    struct sockaddr_in addr;
    socklen_t addr_len;
    time_t last_seen;
} ClientInfo;

// Структура для хранения информации о мониторе
typedef struct {
    struct sockaddr_in addr;
    socklen_t addr_len;
    int registered;
    time_t last_contact;
    int monitor_id;  // Уникальный ID монитора
} MonitorInfo;

#define MAX_MONITORS 10  // Максимальное количество мониторов

TaskPortfolio portfolio = {NULL, 0, 0, 0, NULL};
EncryptedFragment *results = NULL;
ClientInfo *clients = NULL;
MonitorInfo monitors[MAX_MONITORS];  // Массив мониторов
int num_monitors = 0;  // Текущее количество мониторов
int next_monitor_id = 1;  // Следующий ID для нового монитора
int num_clients_connected = 0;
int max_clients = 0;
pthread_mutex_t portfolio_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t results_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t monitors_mutex = PTHREAD_MUTEX_INITIALIZER;  // Мьютекс для мониторов

// Forward declarations для функций, используемых в обработчике SIGINT
void notify_all_clients_finished(int sockfd);
void send_to_all_monitors(int sockfd, const char *event_type, const char *message);
void cleanup_monitors();
void notify_all_clients_shutdown(int sockfd); // Добавлена forward-декларация функции notify_all_clients_shutdown

// Обработчик SIGINT
void sigint_handler(int sig) {
    (void)sig; // Убираем warning о неиспользуемом параметре
    printf("\nПолучен сигнал SIGINT. Завершение работы сервера...\n");
    server_running = 0;
    
    // Уведомляем всех клиентов о завершении работы
    if (sockfd_global != -1) {
        notify_all_clients_shutdown(sockfd_global);
        
        // Отправляем сигнал завершения всем мониторам
        send_to_all_monitors(sockfd_global, MONITOR_SHUTDOWN, "Сервер завершает работу по сигналу SIGINT");
        
        // Очищаем всех мониторов
        cleanup_monitors();
        
        close(sockfd_global);
    }
    
    // Освобождение памяти
    if (portfolio.tasks) {
        for (int i = 0; i < portfolio.total_tasks; i++) {
            free(portfolio.tasks[i]);
        }
        free(portfolio.tasks);
    }
    if (portfolio.original_text) {
        free(portfolio.original_text);
    }
    if (results) {
        for (int i = 0; i < portfolio.total_tasks; i++) {
            if (results[i].encrypted_text) {
                free(results[i].encrypted_text);
            }
        }
        free(results);
    }
    if (clients) {
        free(clients);
    }
    exit(0);
}

// Функция для регистрации нового монитора
void register_monitor(struct sockaddr_in *monitor_addr, socklen_t monitor_len, int sockfd) {
    pthread_mutex_lock(&monitors_mutex);
    
    // Проверяем, есть ли место для нового монитора
    if (num_monitors >= MAX_MONITORS) {
        pthread_mutex_unlock(&monitors_mutex);
        printf("Достигнуто максимальное количество мониторов (%d). Отклоняем подключение от %s:%d\n",
               MAX_MONITORS, inet_ntoa(monitor_addr->sin_addr), ntohs(monitor_addr->sin_port));
        return;
    }
    
    // Проверяем, не подключен ли уже этот монитор
    for (int i = 0; i < num_monitors; i++) {
        if (monitors[i].addr.sin_addr.s_addr == monitor_addr->sin_addr.s_addr &&
            monitors[i].addr.sin_port == monitor_addr->sin_port) {
            // Обновляем время последнего контакта
            monitors[i].last_contact = time(NULL);
            pthread_mutex_unlock(&monitors_mutex);
            
            // Отправляем подтверждение
            if (sendto(sockfd, MONITOR_ACK, strlen(MONITOR_ACK), 0,
                       (struct sockaddr*)monitor_addr, monitor_len) < 0) {
                perror("Ошибка отправки подтверждения монитору");
            } else {
                printf("Монитор #%d переподключился: %s:%d\n",
                       monitors[i].monitor_id, inet_ntoa(monitor_addr->sin_addr), ntohs(monitor_addr->sin_port));
            }
            return;
        }
    }
    
    // Добавляем новый монитор
    monitors[num_monitors].addr = *monitor_addr;
    monitors[num_monitors].addr_len = monitor_len;
    monitors[num_monitors].registered = 1;
    monitors[num_monitors].last_contact = time(NULL);
    monitors[num_monitors].monitor_id = next_monitor_id++;
    
    printf("Новый монитор #%d зарегистрирован: %s:%d (всего мониторов: %d)\n",
           monitors[num_monitors].monitor_id,
           inet_ntoa(monitor_addr->sin_addr), ntohs(monitor_addr->sin_port),
           num_monitors + 1);
    
    num_monitors++;
    pthread_mutex_unlock(&monitors_mutex);
    
    // Отправляем подтверждение монитору
    if (sendto(sockfd, MONITOR_ACK, strlen(MONITOR_ACK), 0,
               (struct sockaddr*)monitor_addr, monitor_len) < 0) {
        perror("Ошибка отправки подтверждения монитору");
    }
}

// Функция для удаления отключенного монитора
void remove_monitor(int index) {
    if (index < 0 || index >= num_monitors) return;
    
    printf("Удаляем отключенный монитор #%d: %s:%d\n",
           monitors[index].monitor_id,
           inet_ntoa(monitors[index].addr.sin_addr),
           ntohs(monitors[index].addr.sin_port));
    
    // Сдвигаем остальные мониторы
    for (int i = index; i < num_monitors - 1; i++) {
        monitors[i] = monitors[i + 1];
    }
    num_monitors--;
}

// Функция для отправки сообщения всем мониторам
void send_to_all_monitors(int sockfd, const char *event_type, const char *message) {
    pthread_mutex_lock(&monitors_mutex);
    
    if (num_monitors == 0) {
        pthread_mutex_unlock(&monitors_mutex);
        return;
    }
    
    // Используем больший буфер для длинных сообщений
    char *full_message = malloc(LARGE_BUFFER_SIZE);
    if (!full_message) {
        pthread_mutex_unlock(&monitors_mutex);
        return;
    }
    
    snprintf(full_message, LARGE_BUFFER_SIZE, "%s:%s", event_type, message);
    
    // Отправляем сообщение всем активным мониторам
    for (int i = 0; i < num_monitors; i++) {
        if (monitors[i].registered) {
            if (sendto(sockfd, full_message, strlen(full_message), 0,
                       (struct sockaddr*)&monitors[i].addr, monitors[i].addr_len) < 0) {
                printf("Ошибка отправки сообщения монитору #%d. Помечаем как отключенный.\n", 
                       monitors[i].monitor_id);
                monitors[i].registered = 0;  // Помечаем как отключенный
            } else {
                monitors[i].last_contact = time(NULL);
            }
        }
    }
    
    // Удаляем отключенные мониторы
    for (int i = num_monitors - 1; i >= 0; i--) {
        if (!monitors[i].registered) {
            remove_monitor(i);
        }
    }
    
    free(full_message);
    pthread_mutex_unlock(&monitors_mutex);
}

// Функция для очистки всех мониторов при завершении
void cleanup_monitors() {
    pthread_mutex_lock(&monitors_mutex);
    num_monitors = 0;
    pthread_mutex_unlock(&monitors_mutex);
}

// Функция для отправки исходного текста всем мониторам
void send_original_text_to_monitors(int sockfd) {
    if (portfolio.original_text) {
        char *monitor_message = malloc(LARGE_BUFFER_SIZE);
        if (monitor_message) {
            snprintf(monitor_message, LARGE_BUFFER_SIZE, "ИСХОДНЫЙ ТЕКСТ: %s", portfolio.original_text);
            send_to_all_monitors(sockfd, SYSTEM_EVENT, monitor_message);
            free(monitor_message);
        }
    }
}

// Функция для чтения файла и создания портфеля задач
int create_task_portfolio(const char *filename, int num_clients) {
    max_clients = num_clients;
    
    // Инициализация массива клиентов
    clients = malloc(max_clients * sizeof(ClientInfo));
    if (!clients) {
        perror("Ошибка выделения памяти для клиентов");
        return -1;
    }
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Ошибка открытия файла");
        return -1;
    }
    
    // Получаем размер файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Читаем весь файл
    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        perror("Ошибка выделения памяти");
        fclose(file);
        return -1;
    }
    
    size_t read_size = fread(file_content, 1, file_size, file);
    file_content[read_size] = '\0';
    fclose(file);
    
    // Сохраняем исходный текст
    portfolio.original_text = malloc(read_size + 1);
    strcpy(portfolio.original_text, file_content);
    
    // Вычисляем размер задачи и количество задач
    portfolio.task_size = file_size / (num_clients * 3);
    portfolio.total_tasks = num_clients * 3;
    
    if (portfolio.task_size == 0) {
        printf("Файл слишком мал для заданного количества клиентов\n");
        free(file_content);
        return -1;
    }
    
    // Создаем массив задач
    portfolio.tasks = malloc(portfolio.total_tasks * sizeof(char*));
    if (!portfolio.tasks) {
        perror("Ошибка выделения памяти для задач");
        free(file_content);
        return -1;
    }
    
    // Создаем массив для результатов
    results = malloc(portfolio.total_tasks * sizeof(EncryptedFragment));
    if (!results) {
        perror("Ошибка выделения памяти для результатов");
        free(portfolio.tasks);
        free(file_content);
        return -1;
    }
    
    // Инициализируем результаты
    for (int i = 0; i < portfolio.total_tasks; i++) {
        results[i].encrypted_text = NULL;
        results[i].position = i;
        results[i].received = 0;
    }
    
    // Разделяем текст на задачи
    for (int i = 0; i < portfolio.total_tasks; i++) {
        portfolio.tasks[i] = malloc(portfolio.task_size + 1);
        if (!portfolio.tasks[i]) {
            perror("Ошибка выделения памяти для задачи");
            // Освобождаем уже выделенную память
            for (int j = 0; j < i; j++) {
                free(portfolio.tasks[j]);
            }
            free(portfolio.tasks);
            free(file_content);
            return -1;
        }
        
        int start_pos = i * portfolio.task_size;
        if (start_pos < (int)read_size) {
            int copy_size = (start_pos + portfolio.task_size <= (int)read_size) ? 
                           portfolio.task_size : (int)read_size - start_pos;
            strncpy(portfolio.tasks[i], file_content + start_pos, copy_size);
            portfolio.tasks[i][copy_size] = '\0';
        } else {
            portfolio.tasks[i][0] = '\0';
        }
    }
    
    free(file_content);
    portfolio.current_task = 0;
    
    printf("=== ПЛЯШУЩИЕ ЧЕЛОВЕЧКИ - ШИФРОВАЛЬНАЯ СИСТЕМА ===\n");
    printf("Исходный текст: %s\n", portfolio.original_text);
    printf("Создан портфель из %d задач по %d символов каждая\n", 
           portfolio.total_tasks, portfolio.task_size);
    return 0;
}

// Функция для получения следующей задачи
char* get_next_task(int *task_position) {
    pthread_mutex_lock(&portfolio_mutex);
    if (portfolio.current_task >= portfolio.total_tasks) {
        pthread_mutex_unlock(&portfolio_mutex);
        return NULL;
    }
    *task_position = portfolio.current_task;
    char *task = portfolio.tasks[portfolio.current_task++];
    pthread_mutex_unlock(&portfolio_mutex);
    return task;
}

// Функция для сохранения зашифрованного результата
void save_encrypted_result(int position, const char *encrypted_text) {
    pthread_mutex_lock(&results_mutex);
    if (position >= 0 && position < portfolio.total_tasks) {
        if (results[position].encrypted_text) {
            free(results[position].encrypted_text);
        }
        results[position].encrypted_text = malloc(strlen(encrypted_text) + 1);
        strcpy(results[position].encrypted_text, encrypted_text);
        results[position].received = 1;
        printf("Получен зашифрованный фрагмент %d: %s\n", position + 1, encrypted_text);
    }
    pthread_mutex_unlock(&results_mutex);
}

// Функция для проверки готовности всех результатов
int all_results_received() {
    pthread_mutex_lock(&results_mutex);
    for (int i = 0; i < portfolio.total_tasks; i++) {
        if (!results[i].received) {
            pthread_mutex_unlock(&results_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&results_mutex);
    return 1;
}

// Функция для вывода итогового результата
void print_final_result(int sockfd) {
    printf("\n=== ИТОГОВЫЙ ЗАШИФРОВАННЫЙ ТЕКСТ ===\n");
    pthread_mutex_lock(&results_mutex);
    
    // Собираем полный зашифрованный текст для отправки на монитор
    // Используем больший буфер для полного результата
    char *full_encrypted_text = malloc(LARGE_BUFFER_SIZE * 2);
    if (full_encrypted_text) {
        full_encrypted_text[0] = '\0';
        
        for (int i = 0; i < portfolio.total_tasks; i++) {
            if (results[i].received) {
                printf("%s", results[i].encrypted_text);
                strcat(full_encrypted_text, results[i].encrypted_text);
            }
        }
        
        // Отправляем итоговый результат на монитор с большим буфером
        char *monitor_message = malloc(LARGE_BUFFER_SIZE * 2);
        if (monitor_message) {
            snprintf(monitor_message, LARGE_BUFFER_SIZE * 2, "ИТОГОВЫЙ ЗАШИФРОВАННЫЙ ТЕКСТ: %s", full_encrypted_text);
            send_to_all_monitors(sockfd, SYSTEM_EVENT, monitor_message);
            free(monitor_message);
        }
        
        free(full_encrypted_text);
    } else {
        for (int i = 0; i < portfolio.total_tasks; i++) {
            if (results[i].received) {
                printf("%s", results[i].encrypted_text);
            }
        }
    }
    
    pthread_mutex_unlock(&results_mutex);
    printf("\n");
}

// Функция для добавления/обновления информации о клиенте
void update_client_info(struct sockaddr_in *client_addr, socklen_t client_len, int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    
    // Ищем существующего клиента
    for (int i = 0; i < num_clients_connected; i++) {
        if (clients[i].addr.sin_addr.s_addr == client_addr->sin_addr.s_addr &&
            clients[i].addr.sin_port == client_addr->sin_port) {
            clients[i].last_seen = time(NULL);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    
    // Добавляем нового клиента
    if (num_clients_connected < max_clients) {
        clients[num_clients_connected].addr = *client_addr;
        clients[num_clients_connected].addr_len = client_len;
        clients[num_clients_connected].last_seen = time(NULL);
        num_clients_connected++;
        printf("Новый клиент подключен: %s:%d (всего клиентов: %d)\n",
               inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), num_clients_connected);
        
        // Уведомляем монитор о новом клиенте
        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "Клиент подключен %s:%d (всего клиентов: %d)",
                inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), num_clients_connected);
        send_to_all_monitors(sockfd, SYSTEM_EVENT, message);
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

// Функция для уведомления всех клиентов о завершении по SIGINT
void notify_all_clients_shutdown(int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    printf("Уведомляем всех клиентов о завершении работы по SIGINT...\n");
    send_to_all_monitors(sockfd, SERVER_EVENT, "Уведомляем всех клиентов о завершении работы по SIGINT");
    for (int i = 0; i < num_clients_connected; i++) {
        if (sendto(sockfd, "SERVER_SHUTDOWN", strlen("SERVER_SHUTDOWN"), 0,
                   (struct sockaddr*)&clients[i].addr, clients[i].addr_len) < 0) {
            perror("Ошибка отправки SERVER_SHUTDOWN клиенту");
        } else {
            printf("SERVER_SHUTDOWN отправлено клиенту %s:%d\n",
                   inet_ntoa(clients[i].addr.sin_addr), ntohs(clients[i].addr.sin_port));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Функция для уведомления всех клиентов о завершении (естественное)
void notify_all_clients_finished(int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    printf("Уведомляем всех клиентов о завершении работы (NO_MORE_TASKS)...\n");
    send_to_all_monitors(sockfd, SERVER_EVENT, "Уведомляем всех клиентов о завершении работы (NO_MORE_TASKS)");
    for (int i = 0; i < num_clients_connected; i++) {
        if (sendto(sockfd, NO_MORE_TASKS, strlen(NO_MORE_TASKS), 0,
                   (struct sockaddr*)&clients[i].addr, clients[i].addr_len) < 0) {
            perror("Ошибка отправки уведомления клиенту");
        } else {
            printf("NO_MORE_TASKS отправлено клиенту %s:%d\n",
                   inet_ntoa(clients[i].addr.sin_addr), ntohs(clients[i].addr.sin_port));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Функция для отображения статистики мониторов
void print_monitors_status() {
    pthread_mutex_lock(&monitors_mutex);
    if (num_monitors > 0) {
        printf("=== АКТИВНЫЕ МОНИТОРЫ ===\n");
        for (int i = 0; i < num_monitors; i++) {
            if (monitors[i].registered) {
                printf("Монитор #%d: %s:%d (последний контакт: %ld сек назад)\n",
                       monitors[i].monitor_id,
                       inet_ntoa(monitors[i].addr.sin_addr),
                       ntohs(monitors[i].addr.sin_port),
                       time(NULL) - monitors[i].last_contact);
            }
        }
        printf("Всего активных мониторов: %d\n", num_monitors);
        printf("========================\n");
    } else {
        printf("Нет подключенных мониторов\n");
    }
    pthread_mutex_unlock(&monitors_mutex);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char *server_ip = NULL;
    int port = DEFAULT_PORT;
    
    // Проверка аргументов
    if (argc < 3) {
        printf("Использование: %s <количество_клиентов> <путь_к_файлу> [IP] [порт]\n", argv[0]);

        exit(1);
    }
    
    int num_clients = atoi(argv[1]);
    if (num_clients <= 0) {
        printf("Количество клиентов должно быть положительным числом\n");
        exit(1);
    }
    
    // Парсинг аргументов IP и порта
    if (argc >= 4) {
        server_ip = argv[3];
        // Проверяем, что это действительно IP адрес, а не порт
        struct sockaddr_in test_addr;
        if (inet_pton(AF_INET, server_ip, &test_addr.sin_addr) <= 0) {
            // Если не IP адрес, возможно это порт
            port = atoi(server_ip);
            server_ip = NULL;  // Слушать на всех интерфейсах
            if (port <= 0 || port > 65535) {
                printf("Некорректный порт. Используется порт по умолчанию: %d\n", DEFAULT_PORT);
                port = DEFAULT_PORT;
            }
        }
    }
    
    if (argc >= 5) {
        port = atoi(argv[4]);
        if (port <= 0 || port > 65535) {
            printf("Некорректный порт. Используется порт по умолчанию: %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }
    
    // Установка обработчика сигналов
    signal(SIGINT, sigint_handler);
    
    // Создание портфеля задач
    if (create_task_portfolio(argv[2], num_clients) < 0) {
        exit(1);
    }
    
    // Создание UDP сокета
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        exit(1);
    }
    
    sockfd_global = sockfd;
    
    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    
    // Установка IP адреса
    if (server_ip != NULL) {
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            printf("Некорректный IP адрес: %s. Слушаем\n", server_ip);
            server_addr.sin_addr.s_addr = INADDR_ANY;
        }
    } else {
        server_addr.sin_addr.s_addr = INADDR_ANY;  
    }
    
    server_addr.sin_port = htons(port);
    
    // Привязка сокета к адресу
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка привязки");
        close(sockfd);
        exit(1);
    }
    
    // Вывод информации о запуске сервера
    if (server_ip != NULL) {
        printf("Сервер запущен на %s:%d\n", server_ip, port);
    } else {
        printf("Сервер запущен на всех интерфейсах, порт %d\n", port);
    }
    printf("Ожидание запросов задач от клиентов...\n");
    printf("Поддерживается до %d мониторов одновременно\n", MAX_MONITORS);
    print_monitors_status();
    
    while (server_running) {
        client_len = sizeof(client_addr);
        
        // Получение запроса от клиента
        int received_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                                     (struct sockaddr*)&client_addr, &client_len);
        
        if (received_bytes < 0) {
            if (server_running) {
                perror("Ошибка получения данных");
            }
            continue;
        }
        
        buffer[received_bytes] = '\0';
        printf("Получен запрос от клиента: %s\n", buffer);
        
        // Проверяем, это регистрация монитора
        if (strcmp(buffer, MONITOR_REGISTER) == 0) {
            register_monitor(&client_addr, client_len, sockfd);
            // Отправляем исходный текст сразу после регистрации
            send_original_text_to_monitors(sockfd);
            // Выводим обновленную статистику мониторов
            print_monitors_status();
            continue;
        }
        
        // Обновляем информацию о клиенте
        update_client_info(&client_addr, client_len, sockfd);
        
        // Проверяем, что это запрос задачи
        if (strcmp(buffer, REQUEST_TASK) == 0) {
            int task_position;
            char *task = get_next_task(&task_position);
            
            if (task == NULL) {
                // Задачи закончились
                strcpy(response, NO_MORE_TASKS);
                printf("Задачи закончились. Отправляем сигнал завершения клиенту\n");
                send_to_all_monitors(sockfd, SERVER_EVENT, "Задачи закончились - отправляем сигнал завершения клиенту");
            } else {
                // Формируем ответ: позиция:задача
                snprintf(response, BUFFER_SIZE, "%d:%s", task_position, task);
                printf("Отправляем задачу клиенту (позиция %d): %s\n", task_position, task);
                
                char message[BUFFER_SIZE];
                snprintf(message, BUFFER_SIZE, "Отправлена задача %d клиенту %s:%d", 
                        task_position + 1, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                send_to_all_monitors(sockfd, SERVER_EVENT, message);
            }
            
            // Отправка ответа клиенту
            if (sendto(sockfd, response, strlen(response), 0,
                       (struct sockaddr*)&client_addr, client_len) < 0) {
                perror("Ошибка отправки данных");
            }
        }
        // Проверяем, что это отправка результата
        else if (strncmp(buffer, SUBMIT_RESULT, strlen(SUBMIT_RESULT)) == 0) {
            // Парсим результат: SUBMIT_RESULT:позиция:зашифрованный_текст
            char *token = strtok(buffer, ":");
            if (token && strcmp(token, SUBMIT_RESULT) == 0) {
                token = strtok(NULL, ":");
                if (token) {
                    int position = atoi(token);
                    token = strtok(NULL, "");  // Остаток строки
                    if (token) {
                        save_encrypted_result(position, token);
                        
                        // Уведомляем монитор о получении результата
                        char message[BUFFER_SIZE];
                        snprintf(message, BUFFER_SIZE, "Получен результат фрагмента %d от клиента %s:%d", 
                                position + 1, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        send_to_all_monitors(sockfd, SERVER_EVENT, message);
                        
                        // Отправляем подтверждение
                        strcpy(response, TASK_RECEIVED);
                        if (sendto(sockfd, response, strlen(response), 0,
                                   (struct sockaddr*)&client_addr, client_len) < 0) {
                            perror("Ошибка отправки подтверждения");
                        }
                        
                        // Проверяем, все ли результаты получены
                        if (all_results_received()) {
                            print_final_result(sockfd);
                            send_to_all_monitors(sockfd, SYSTEM_EVENT, "Все фрагменты получены - формируется итоговый результат");
                            notify_all_clients_finished(sockfd);
                            printf("Все фрагменты обработаны. Сервер завершает работу.\n");
                            send_to_all_monitors(sockfd, SYSTEM_EVENT, "Сервер завершает работу");
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Отправляем сигнал завершения монитору
    send_to_all_monitors(sockfd, MONITOR_SHUTDOWN, "Сервер завершает работу");
    
    close(sockfd);
    
    // Освобождение памяти
    if (portfolio.tasks) {
        for (int i = 0; i < portfolio.total_tasks; i++) {
            free(portfolio.tasks[i]);
        }
        free(portfolio.tasks);
    }
    if (portfolio.original_text) {
        free(portfolio.original_text);
    }
    if (results) {
        for (int i = 0; i < portfolio.total_tasks; i++) {
            if (results[i].encrypted_text) {
                free(results[i].encrypted_text);
            }
        }
        free(results);
    }
    if (clients) {
        free(clients);
    }
    
    return 0;
}
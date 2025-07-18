# ИДЗ 4

## ФИО и группа

**Исполнитель:** Зенин Вадим Вадимович  
**Группа:** БПИ237

---

## Номер варианта и условие задачи

**Вариант:** 33

**Условие задачи:**  
**Пляшущие человечки.** На тайном собрании глав преступного
мира города Лондона председатель собрания профессор Мориарти
постановил: отныне вся переписка между преступниками должна
вестись тайнописью. В качестве стандарта были выбраны «пляшу-
щие человечки», шифр, в котором каждой букве латинского алфа-
вита соответствует хитроумный значок.
Реализовать клиент–серверное приложение, шифрующее
исходный текст (в качестве ключа используется кодовая
таблица, устанавливающая однозначное соответствие
между каждой буквой и каким–нибудь числом).

Каждый процесс–шифровальщик является клиентом. Он кодиру-
ет свои кусочки общего текста. При решении использовать па-
радигму портфеля задач. клиенты работают асинхронно, форми-
руя свой закодированный фрагмент в случайное время. Следова-
тельно, при занесении информации в портфель–сервер необходи-
мо проводить упорядочение фрагментов.

В программе необходимо вывести исходный текст, закодированные
фрагменты по мере их формирования, окончательный закодиро-
ванный текст.
---

## Сценарий решаемой задачи

1. Шифрование представлено в виде таблицы ASCII, в которой уже каждому символу соответствует свое число.
2. Процесс шифрование заключается в переводе текста в аски коды.
3. Реализации задачи использует парадигму портфель задач. Задача - кодировка текста в аски. При запуске сервера, ему в аргументы передается количество клиентов, для того чтобы понять как делить текст на задачи для клиентов, я решил, что пусть всего будет по 3 задачи на клиента, в среднем. Те размер задачи = `размер файла / (количество клиентов * 3)`. Также на вход подается `абсолютный путь` до файла с текстом. Сервер читает файл и потом разбивает его на фрагменты, которые индексирует и передает клиентам. 
4. Сервер и клиенты работают на IP и портах введенных через консоль.
5. Клиенты отправляют зашиврованные фрагменты обратно серверу, чтобы он мог их сопоставить обратно, а также выводят зашифрованные куски на консоль.
6. Клиенты и сервер могут корректно завершить работу на `SIGINT`, а также после завершения задач.

---

## 4-5

### Запуск сервера
```bash
./server <количество_клиентов> <путь_к_файлу> [IP] [порт]
```

Примеры:
```bash
./server 2 secret_message.txt         # порт 127.0.0.1:8080 по умолчанию
./server 3 document.txt 9000          # порт 127.0.0.1:9000
```

### Запуск клиента
```bash
./client [IP_сервера] [порт]
```

Примеры:
```bash
./client                              # localhost:8080 по умолчанию
./client 192.168.1.100                # указанный IP, порт 8080
./client 192.168.1.100 9000           # указанный IP и порт
```

## Вывод программы

### Сервер показывает:
```
=== ПЛЯШУЩИЕ ЧЕЛОВЕЧКИ - ШИФРОВАЛЬНАЯ СИСТЕМА ===
Исходный текст: Hello World! This is a secret message from Professor Moriarty to all the members of his criminal organization. 
AHAHAHAHHAHAHAHAHAHHAHAHAHHAHAHAHAA...
Создан портфель из 6 задач по 156 символов каждая
Сервер запущен на 127.0.0.1:8080
Ожидание запросов задач от клиентов...
Получен запрос от клиента: GET_TASK
Новый клиент подключен: 127.0.0.1:51667 (всего клиентов: 1)
Отправляем задачу клиенту (позиция 0): Hello World! This is a secret message from Professor Moriarty to all the members of his criminal organization. 
Получен запрос от клиента: SUBMIT_RESULT:0:72 101 108 108 111 32 87 111 114 108 100 33 32 84 104 105 115 32 105 115 32 97 32 115 101 99 114 101 116 32 109 1
=== ИТОГОВЫЙ ЗАШИФРОВАННЫЙ ТЕКСТ ===
72 101 108 108 111 32 87 111 114 108 100 33 32 84 104 105 115 32 105 115 32 97 32 115 101 99 114 101 116 32 109 101 115 115 97 103 101 32 102 114 111 109 32 80 114 111 102 101 115 115 111 114 32 77 111 114 ...
```

### Клиент показывает:
```
=== КЛИЕНТ-ШИФРОВАЛЬЩИК ПЛЯШУЩИХ ЧЕЛОВЕЧКОВ ===
Подключение к серверу 127.0.0.1:8080

Получена задача 1 (позиция 0): Hello World! This is a secret message from Professor Moriarty to all the members of his criminal organization. 
AHAHAHAHHAHAHAHAHAHHAHAHAHHAHAHAHAAHAHAHAHHA
Шифрование фрагмента...
Зашифрованный фрагмент: 72 101 108 108 111 32 87 111 114 108 100 33 32 84 104 105 115 32 105 115 32 97 32 115 101 99 114 101 116 32 109 101 115 115 97 103 101 32 102 114 111 109 32 80 114 111 102 101 115 115 111 114 32 77 111 114 105 97 114 116 121 32 116 111 32 97 108 108 32 116 104 101 32 109 101 109 98 101 114 115 32 111 102 32 104 105 115 32 99 114 105 109 105 110 97 108 32 111 114 103 97 110 105 122 97 116 105 111 110 46 32 10 65 72 65 72 65 72 65 72 72 65 72 65 72 65 72 65 72 65 72 72 65 72 65 72 65 72 72 65 72 65 72 65 72 65 65 72 65 72 65 72 65 72 72 65 
Результат отправлен серверу
```

## Завершение работы
- **Ctrl+C**: корректное завершение с освобождением памяти
- **Автоматическое**: после обработки всех фрагментов
- **Сетевая устойчивость**: обработка разрывов соединения

---

# 6-7

## Изменения

Добавлен монитор, по анологии с предыдущим ИДЗ, которые подключается к серверу по тому же порту что и клиент и выводит на консоль логи системы, также корректно обрабатывает `SIGINT`.

## Пример работы

1. Запускаем сервер.
2. Запускаем монитор.

```bash
./monitor [IP] [порт]
```
3. Запускаем клиентов.

### Выыод монитора

```
=== МОНИТОР СИСТЕМЫ ШИФРОВАНИЯ ===
Подключение к серверу 127.0.0.1:8080
[16:43:59] Запрос на регистрацию монитора отправлен
[16:43:59] Монитор успешно зарегистрирован на сервере
[16:43:59] Начинаем отслеживание событий системы...

=== СТАТИСТИКА СИСТЕМЫ [16:43:59] ===
Монитор активен и отслеживает события системы
==========================================

[16:43:59] ИСХОДНЫЙ ТЕКСТ: Hello World! This is a secret message from Professor Moriarty to all the members of his criminal organization. 
AHAHAHAHHAHAH...
[16:44:03] СИСТЕМА: Клиент подключен 127.0.0.1:58650 (всего клиентов: 1)
[16:44:03] СЕРВЕР: Отправлена задача 1 клиенту 127.0.0.1:58650
[16:44:03] СИСТЕМА: Клиент подключен 127.0.0.1:62174 (всего клиентов: 2)
[16:44:04] СЕРВЕР: Получен результат фрагмента 1 от клиента 127.0.0.1:58650
[16:44:04] СЕРВЕР: Отправлена задача 2 клиенту 127.0.0.1:58650
[16:44:04] СЕРВЕР: Получен результат фрагмента 2 от клиента 127.0.0.1:58650
[16:44:04] СЕРВЕР: Отправлена задача 3 клиенту 127.0.0.1:58650 ...
[16:44:06] ИТОГОВЫЙ ЗАШИФРОВАННЫЙ ТЕКСТ: 72 101 108 108 111 32 87 111 114 108 100 33 32 84 104 105 115 32 105 115 32 97 32 115 101 99 114 101 116 32 109 101 115 115 97 103 101 32 102 114 111 109 32 80 114 111 102 101 115 115 111 114 32 77 111 114 105 97 114 116 121 32 116 111 32 97 108 108 32 116 104 101 32 109 101 109 98 101 114 115 32 111 102 32 104 105 115 32 99 114 105 109 105 110 97 108 32 111 114 103 97 110 105 122 ...
[16:44:06] СИСТЕМА: Все фрагменты получены - формируется итоговый результат
[16:44:06] СЕРВЕР: Уведомляем всех клиентов о завершении работы
[16:44:06] СЕРВЕР: Уведомление о завершении отправлено клиенту 127.0.0.1:58650
[16:44:06] СЕРВЕР: Уведомление о завершении отправлено клиенту 127.0.0.1:62174
[16:44:06] СИСТЕМА: Сервер завершает работу
[16:44:06] ЗАВЕРШЕНИЕ: Сервер завершает работу
[16:44:06] Сервер завершил работу. Монитор автоматически завершается.
[16:44:06] Монитор завершил работу
```

---

# 8

## Краткое резюме изменений

### Что было изменено в server.c:

1. **Структуры данных:**
   - Заменил одиночный `MonitorInfo monitor` на массив `MonitorInfo monitors[MAX_MONITORS]`
   - Добавил поля `monitor_id`, `num_monitors`, `next_monitor_id`


2. **Новые функции:**
   - `register_monitor()` -> поддержка множественных подключений с уникальными ID
   - `remove_monitor()` -> удаление отключенного монитора из массива
   - `send_to_all_monitors()` -> рассылка сообщений всем активным мониторам
   - `cleanup_monitors()` -> очистка всех мониторов при завершении
   - `print_monitors_status()` -> статистика активных мониторов

3. **Модифицированные функции:**
   - `send_original_text_to_monitors()` -> отправка исходного текста всем мониторам
   - `sigint_handler()` -> корректное завершение всех мониторов по SIGINT
   - `main()` -> вывод статистики при регистрации новых мониторов


## Результаты:
Теперь можно подключать несколько мониторов по тем же сокетам, что и раньше. 
**Множественные мониторы**: до 10 мониторов одновременно  
**Динамическое управление**: подключение/отключение в реальном времени  
**Уникальная идентификация**: каждый монитор получает свой ID  
**Автоматическая очистка**: удаление неактивных мониторов  
**Статистика**: отображение всех активных подключений  

## Пример работы:

Вывод у мониторов абсолютно одинаковый. 
Вывод сервера:
```
Создан портфель из 6 задач по 156 символов каждая
Сервер запущен на всех интерфейсах, порт 8080
Ожидание запросов задач от клиентов...
Поддерживается до 10 мониторов одновременно
Нет подключенных мониторов.
Получен запрос от клиента: REGISTER_MONITOR
Новый монитор #1 зарегистрирован: 127.0.0.1:52877 (всего мониторов: 1)
=== АКТИВНЫЕ МОНИТОРЫ ===
Монитор #1: 127.0.0.1:52877 (последний контакт: 0 сек назад)
Всего активных мониторов: 1
========================

Получен запрос от клиента: REGISTER_MONITOR
Новый монитор #2 зарегистрирован: 127.0.0.1:54856 (всего мониторов: 2)
=== АКТИВНЫЕ МОНИТОРЫ ===
Монитор #1: 127.0.0.1:52877 (последний контакт: 0 сек назад)
Монитор #2: 127.0.0.1:54856 (последний контакт: 0 сек назад)
Всего активных мониторов: 2
========================
```
и тд.

# 9-10

## Дополнение работы

Предыдущая реализация позволяет использовать механизм, прекращения работы конкретных клиентов и их последующего возвращения. 
Для корректного отображения деталей в логах, добавил небольшой слип клиентам для демонстрации работы (потом уберу).
Вот выыод работы монитора, при подключении и отключении различных клиентов
```
[00:11:02] СИСТЕМА: Клиент подключен 127.0.0.1:57312 (всего клиентов: 1)
[00:11:02] СЕРВЕР: Отправлена задача 1 клиенту 127.0.0.1:57312
[00:11:02] СИСТЕМА: Клиент подключен 127.0.0.1:62289 (всего клиентов: 2)
[00:11:02] СЕРВЕР: Получен результат фрагмента 1 от клиента 127.0.0.1:57312
[00:11:04] СЕРВЕР: Отправлена задача 2 клиенту 127.0.0.1:52329
[00:11:04] СЕРВЕР: Получен результат фрагмента 2 от клиента 127.0.0.1:52329
[00:11:06] СЕРВЕР: Отправлена задача 3 клиенту 127.0.0.1:52244
[00:11:06] СЕРВЕР: Получен результат фрагмента 3 от клиента 127.0.0.1:52244
[00:11:11] СЕРВЕР: Отправлена задача 4 клиенту 127.0.0.1:52244
[00:11:12] СЕРВЕР: Получен результат фрагмента 4 от клиента 127.0.0.1:52244
[00:11:17] СЕРВЕР: Отправлена задача 5 клиенту 127.0.0.1:52244
[00:11:17] СЕРВЕР: Получен результат фрагмента 5 от клиента 127.0.0.1:52244
[00:11:22] СЕРВЕР: Отправлена задача 6 клиенту 127.0.0.1:52244
[00:11:23] СЕРВЕР: Получен результат фрагмента 6 от клиента 127.0.0.1:52244
```

Также для выполнения задания на 10 было немного модифицирован процесс завершения работы, в предыдущих версия работа клиентов завершалась, когда кончались задачи, сервер отправлял им специальное сообщение и сам завершал работу.
Сейчас добавил метод, который еще и обрабатывает  `SGINT`, после его получения сервер по той же схеме отправляет клиентам и мониторам сообщение и завершает работу.

**Вывод клиентов**
```
Подтверждение получено от сервера
Сервер завершает работу. Клиент завершает работу.
Клиент обработал 1 фрагментов.
```

**Вывод сервера**
```
^C
Получен сигнал SIGINT. Завершение работы сервера...
Уведомляем всех клиентов о завершении работы по SIGINT...
SERVER_SHUTDOWN отправлено клиенту 127.0.0.1:64796
SERVER_SHUTDOWN отправлено клиенту 127.0.0.1:52914
```
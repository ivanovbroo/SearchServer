#include <iostream>
#include <stdexcept>

#include "request_queue.h"
#include "log_duration.h"
#include "remove_duplicates.h"
#include "process_queries.h"

using namespace std;

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string_view& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, string_view document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, string_view raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            cout << document << endl;
        }
    }
    catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, string_view query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

template <typename Type>
ostream& operator<<(ostream& out, const vector<Type>& container) {
    for (const auto& i : container)
        out << i << " ";
    return out;
}

template <typename Type>
void TestSearchServerСonstructor(const Type& stop_words) {
    try {
        cout << "Введенное стоп-слово: "s << stop_words << endl;
        SearchServer search_server(stop_words);
    }
    catch (const invalid_argument& stop_words) {
        cout << "Error: "s << stop_words.what() << endl;
    }
}

void TestGetDocumentId(const SearchServer& search_server, int id) {
    cout << "Введенное id: "s << id << endl;
    try {
        search_server.GetDocumentId(id);
    }
    catch (const invalid_argument& stop_words) {
        cout << "Error: "s << stop_words.what() << endl;
    }
}

int main() {
    setlocale(LC_ALL, "Russian");

    /* Добавление слов/предлогов через конструктор, которые не нужно учитывать */
    SearchServer search_server("и в на"s);

    /* Cколько за последние сутки было запросов? */
    RequestQueue request_queue(search_server);

    /* Добавление текста в документы */
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "большой кот модный ошейник "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "большой пёс скворец василий"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    /* Тест на проверку итераторов */
    cout << "Проверка итераторов:" << endl;
    for (const int document_id : search_server) {
        cout << document_id << endl;
    }
    cout << endl;

    /* Тест GetWordFrequencies */
    cout << "GetWordFrequencies: " << endl;
    for (const auto& [word, frequencies] : search_server.GetWordFrequencies(1)) {
        cout << word << " = " << frequencies << endl;
    }
    cout << endl;
    //проверка на возвращение пустой мапы
    for (const auto& [word, frequencies] : search_server.GetWordFrequencies(10)) {
        cout << word << " = " << frequencies << endl;
    }
    cout << endl;

    /* Cколько за последние сутки было запросов? */
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("пустой запрос"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("пушистый пёс"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("большой ошейник"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("скворец"s);
    cout << "Запросов, по которым ничего не нашлось "s << request_queue.GetNoResultRequests() << endl;
    cout << endl;

    /*Основная функция поиска самых подходящих документов по запросу. Возвращает вектор структуры Document;
     *Выводит номер документа, где находится слово по запросу, релевантность слов по документам, рейтинг документа */
    cout << "FindTopDocuments:" << endl;
    FindTopDocuments(search_server, "кот пёс"s);
    cout << endl;

    /*
     * Функция, которая возвращает кортеж из вектора совпавших слов из запроса в документе.
     * Если таких нет или совпало хоть одно минус слово, кортеж возвращается с пустым вектором слов
     * и статусом документа.
     */
    cout << "MatchDocuments:" << endl;
    MatchDocuments(search_server, "кот пёс"s);
    cout << endl;

    /* Проверка разбиения строки на слова, возвращает вектор слов */
    cout << "SplitIntoWords:" << endl;
    for (string_view word : SplitIntoWords("кот пёс птица синица"s)) {
        cout << word << endl;
    }
    cout << endl;

    cout << "SplitIntoWords2:" << endl;
    for (const string& word : SplitIntoWords2("кот пёс птица синица"s)) {
        cout << word << endl;
    }
    cout << endl;
    
    /* Разбиение результатов на страницы */
    cout << "Paginate:" << endl;
    const auto search_results = search_server.FindTopDocuments("пушистый пёс"s);
    //задаем переменной сколько поместить на странце результатов. В данном случае на одной странице будет 2 вектора.
    int page_size = 2;
    //найдет три вектора по запросу "пушистый пёс" => 1 стр = 2 вектора; 2 стр = 1 вектор
    FindTopDocuments(search_server, "пушистый пёс"s);
    cout << endl;
    const auto pages = Paginate(search_results, page_size);
    cout << endl;

    // Выводим найденные документы по страницам
    for (auto page = pages.begin(); page != pages.end(); ++page) {
        cout << *page << endl;
        cout << "Разрыв страницы"s << endl;
    }
    cout << endl;

    /* Тест ловим исключение при добавлении стоп-слов */
    cout << "TestSearchServerСonstructor:" << endl;
    const vector<string> stop_words_1 = { "слово"s, "скво\x12рец"s };
    TestSearchServerСonstructor(stop_words_1);
    cout << endl;

    const vector<string> stop_words_2 = { "слово"s };
    TestSearchServerСonstructor(stop_words_2);
    cout << endl;

    TestSearchServerСonstructor("скво\x12рец"s);
    cout << endl;

    /* Тест проверяем есть ли такой индекс в документе*/
    cout << "TestGetDocumentId: " << endl;
    int id_1 = -1;
    TestGetDocumentId(search_server, id_1);
    cout << endl;

    int id_2 = 0;
    TestGetDocumentId(search_server, id_2);
    cout << endl;

    /*Нельзя использовать: неккоректные символы, отрицательные значения, повтор документа, --, - , -_*/
    SearchServer search_server_test5("и в на"s);
    cout << "AddDocument:" << endl;
    AddDocument(search_server_test5, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server_test5, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server_test5, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server_test5, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    AddDocument(search_server_test5, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 1, 1 });
    cout << endl;

    cout << "FindDocument:" << endl;
    FindTopDocuments(search_server_test5, "скво\x12рец -пёс"s);
    cout << endl;
    FindTopDocuments(search_server_test5, "пушистый -скво\x12рец"s);
    cout << endl;
    FindTopDocuments(search_server_test5, "пушистый -пёс"s);
    cout << endl;
    FindTopDocuments(search_server_test5, "пушистый --кот"s);
    cout << endl;
    FindTopDocuments(search_server_test5, "пушистый -"s);
    cout << endl;

    cout << "MatchDocuments:" << endl;
    MatchDocuments(search_server_test5, "пушистый пёс"s);
    cout << endl;
    MatchDocuments(search_server_test5, "модный -кот"s);
    cout << endl;
    MatchDocuments(search_server_test5, "модный --пёс"s);
    cout << endl;
    MatchDocuments(search_server_test5, "пушистый - хвост"s);
    cout << endl;

    /* Тест времени */
    {
        LOG_DURATION_STREAM("Operation time", cout);
        MatchDocuments(search_server, "пушистый -пёс"s);
    }
    cout << endl;
    {
        LOG_DURATION_STREAM("Operation time", cout);
        FindTopDocuments(search_server, "кот пёс"s);
    }
    cout << endl;

    /* Тест RemoveDuplicates */
    SearchServer search_server_dupl("and with"s);

    AddDocument(search_server_dupl, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server_dupl, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // дубликат документа 2, будет удалён
    AddDocument(search_server_dupl, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server_dupl, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server_dupl, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // добавились новые слова, дубликатом не является
    AddDocument(search_server_dupl, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server_dupl, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // есть не все слова, не является дубликатом
    AddDocument(search_server_dupl, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // слова из разных документов, не является дубликатом
    AddDocument(search_server_dupl, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    cout << "Before duplicates removed: "s << search_server_dupl.GetDocumentCount() << endl;
    RemoveDuplicates(search_server_dupl);
    cout << "After duplicates removed: "s << search_server_dupl.GetDocumentCount() << endl;
    cout << endl;

    
    /* Тест process_quries */
    {
        cout << "Тест ProcessQueries:" << endl;
        SearchServer search_server_pq("and with"s);

        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
            ) {
            search_server_pq.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }

        const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s
        };
        id = 0;
        for (const auto& documents : ProcessQueries(search_server_pq, queries)) {
            cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
        }
    }
    cout << endl;

    

    /* Тест ProcessQueriesJoined */
    {
        cout << "Тест ProcessQueriesJoined:" << endl;

        SearchServer search_server_pqj("and with"s);

        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
            ) {
            search_server_pqj.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }

        const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s
        };
        for (const Document& document : ProcessQueriesJoined(search_server_pqj, queries)) {
            cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
        }
    }
    cout << endl; 

    /* Тест RemoveDocuments на параллельность и последовательность */
    {
        cout << "Тест RemoveDocuments на параллельность и последовательность:" << endl;
        SearchServer search_server_rd("and with"s);

        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
            ) {
            search_server_rd.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }

        const string query = "curly and funny"s;

        auto report = [&search_server_rd, &query] {
            cout << search_server_rd.GetDocumentCount() << " documents total, "s
                << search_server_rd.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << endl;
        };

        report();
        // однопоточная версия
        search_server_rd.RemoveDocument(5);
        report();
        // однопоточная версия

        search_server_rd.RemoveDocument(execution::seq, 1);
        report();
        // многопоточная версия
        search_server_rd.RemoveDocument(execution::par, 2);
        report();
    }
    cout << endl;

    /* Тест MatchDocument на параллельность и последовательность */
    {
        cout << "Тест MatchDocument на параллельность и последовательность:"s << endl;
        SearchServer search_server("and with"s);

        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
            ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }

        const string query = "curly and funny -not"s;

        {
            LOG_DURATION_STREAM("Operation time", cout);
            const auto [words, status] = search_server.MatchDocument(query, 1);
            cout << words.size() << " words for document 1"s << endl;
            // 1 words for document 1
        }

        {
            LOG_DURATION_STREAM("Operation time", cout);
            const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
            cout << words.size() << " words for document 2"s << endl;
            // 2 words for document 2
        }

        {
            LOG_DURATION_STREAM("Operation time", cout);
            const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
            cout << words.size() << " words for document 3"s << endl;
            // 0 words for document 3
        }
    }
    cout << endl;

    /* Тест на параллельный поиск документов */
    cout << "Тест на параллельный поиск документов:" << endl;
    SearchServer search_server_par("and with"s);

    int id = 0;
    for (
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }
        ) {
        search_server_par.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }


    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server_par.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server_par.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server_par.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
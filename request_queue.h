#pragma once

#include <deque>				

#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server_);

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    /*отвечает на вопрос : сколько за последние сутки было запросов, на которые ничего не нашлось ?*/
    int GetNoResultRequests() const;

private:
    /*структура для того, чтобы хранить в деке данные: 1 - в какое время был сделан запрос
                                                       2 - результат нахождения запроса(пустой или не пустой)*/
    struct QueryResult {
        uint64_t time_step;
        size_t result;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& search_server_;
    int no_search_result_;                  //переменная для счета не найденных результатов
    uint64_t current_time_;


    void AddRequest(size_t result);
};

template <typename DocumentPredicate>
std::vector<Document>RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    auto found_result = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddRequest(found_result.size());
    return found_result;
}
#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
    , no_search_result_(0)
    , current_time_(0) {
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    auto found_result = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(found_result.size());
    return found_result;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    auto found_result = search_server_.FindTopDocuments(raw_query);
    AddRequest(found_result.size());
    return found_result;
}

/*отвечает на вопрос : сколько за последние сутки было запросов, на которые ничего не нашлось ?*/
int RequestQueue::GetNoResultRequests() const {
    return no_search_result_;
}

void RequestQueue::AddRequest(size_t result) {
    ++current_time_;
    /*хитроумный цикл: будет удалять старые результаты, когда наступят новые сутки, то есть > 1440 сек*/
    while (!requests_.empty() && sec_in_day_ <= current_time_ - requests_.front().time_step) {
        /*отвечает на вопрос: сколько за последние сутки было запросов, на которые ничего не нашлось? в данном случае в текущий день*/
        if (requests_.front().result == 0) {
            --no_search_result_;
        }
        requests_.pop_front();
    }
    // сохраняем новый результат поиска
    requests_.push_back({ current_time_, result });
    if (result == 0) {
        ++no_search_result_;
    }
}
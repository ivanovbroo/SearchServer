#pragma once

#include <algorithm>		
#include <map>
#include <numeric>
#include <utility>
#include <execution>
#include <string_view>

#include "document.h"
#include "string_processing.h"
#include "paginator.h"
#include "concurrent_map.h"


const int MAX_RESULT_DOCUMENT_COUNT = 5;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer {
public:

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    explicit SearchServer(std::string_view stop_words_text);


    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    /*
     *
     * Основная функция поиска самых подходящих документов по запросу.
     * Для уточнения поиска используется функция лямбда.
     *
     */

    //однопоточная
    template<typename DocumentSort>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentSort document_sort) const;
    
    //общая функция которая принимает поточную/многопоточную версию
    template<typename ExecutionPolicy, typename DocumentSort>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy policy, std::string_view raw_query, DocumentSort document_sort) const;

    /*
    *
    * Перегрузка функции поиска с использованием статуса в качестве второго парметра
    *
    */
    
    //однопоточная
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    //многопоточная/однопоточная
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const;

    //однопоточная
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    //многопоточная/однопоточная
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    /*
     *
     * Функция, которая возвращает кортеж из вектора совпавших слов из raw_query в документе document_id.
     * Если таких нет или совпало хоть одно минус слово, кортеж возвращается с пустым вектором слов
     * и статусом документа.
     *
     */

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&,
        std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&,
        std::string_view raw_query, int document_id) const;


    int GetDocumentId(int index) const;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy&, int document_id);




private:
    struct DocumentInformation {
        int rating;
        DocumentStatus document_status;
        //Сохраняем тексты документов для создания
        std::string text;               
    };

    const TransparentStringSet stop_words_;
    //Сохраняем тексты документов для создания
    std::vector<std::string> save_text;
    std::map<std::string_view, double> empty_map;

    std::map<int, DocumentInformation> documents_data_;
    std::set<int> document_ids_;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> id_word_frequencies_;


    bool IsStopWord(std::string_view word) const;

    /*
     *
     * проверка слова на неккоректные символы
     *
     */

    static bool IsValidWord(std::string_view word);

    /*
    *
    * разбивает на слова при учитывании стоп слов
    *
    */
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    /*
    *
    * вычисление среднего рейтинга
    *
    */

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    /*
     *
     * Анализ слова
     * Возвращает структуру со свойствами слова word
     *
     */

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    /*
     *
     * Разбивает строку-запрос на плюс и минус слова, исключая стоп слова.
     * Возвращает структуру с двумя множествами этих слов.
     *
     */

    Query ParseQuery(std::string_view text) const;

    /*
     *
     * Вычисление IDF слова
     *
     */

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    /*
     *
     * Нахождение документов по запросу(в каких документах находтся слова при учитовании минус слов) + вычисление IDF
     *
     */

    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query) const;

    std::vector<Document> FindAllDocuments(const Query& query) const;

    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query) const;

};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Incorrect text input");
    }
}

template<typename DocumentSort>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentSort document_sort) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_sort);
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus input_status, int rating) {
        return input_status == status;
    });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}


template<typename ExecutionPolicy, typename DocumentSort>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy policy, std::string_view raw_query, DocumentSort document_sort) const {

    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query);

/// для параллельного случая сортировка так же долна быть параллельной
/// не проверял, но возможно будет достаточно передать policy в параллельную версию sort
    std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });

    std::vector<Document> matched_documents_reborn;
    for (const auto& document : matched_documents) {
        if (document_sort(document.id, documents_data_.at(document.id).document_status, document.rating)) {
            matched_documents_reborn.push_back(document);
        }
        else {   /// лишний оператор
            continue;
        }
    }

    if (matched_documents_reborn.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents_reborn.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents_reborn;
}

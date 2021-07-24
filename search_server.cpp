#include <stdexcept>
#include <cmath>
#include <execution>
#include <string_view>


#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(string_view(stop_words_text))
{
}

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_data_.count(document_id) > 0)) {
        throw invalid_argument("Your id is negative or already exists");
    }
   
    const auto it_inserted_word = save_text.emplace(save_text.end(), string(document));
    const auto words = SplitIntoWordsNoStop(*it_inserted_word);

    documents_data_.emplace(document_id, DocumentInformation{ ComputeAverageRating(ratings), status, *it_inserted_word });

    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_word_frequencies_[document_id][word] += inv_word_count;
    }
    
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query);
}

int SearchServer::GetDocumentCount() const {
    return documents_data_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&, string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string_view> words;

    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { words, documents_data_.at(document_id).document_status };
        }
    }

    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto &[id, _] : word_to_document_freqs_.at(word)) {
            if (document_id == id)
                words.push_back(word);
        }
    }

    return { words, documents_data_.at(document_id).document_status };  
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&, string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string_view> matched_words;

    const auto status = documents_data_.at(document_id).document_status;

    //функция которая проверяет, есть ли в множетве минус/плюс слов слова в общей базе данных и соотвественно id документа
    const auto word_checker =
        [this, document_id](string_view word) {
        const auto it = word_to_document_freqs_.find(word);
        return it != word_to_document_freqs_.end() && it->second.count(document_id);
    };

    //проходимся по всему диапозону минус слов и проверяем с помощью функции word_checker есть ли минус слово в базе данных
    if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(), word_checker)) {
        return { matched_words, status };
    }

    matched_words.resize(query.plus_words.size());

    //скопируем все плюс слова в отдельный объект с помощью функции word_checker
    auto words_end = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), word_checker);
    sort(matched_words.begin(), words_end);
    
    //удалим все повторяющие слова в векторе, например (word1, word1, word2) -> (word1, word2) запищется в words_end
    words_end = unique(matched_words.begin(), words_end);
    //удаляем
    matched_words.erase(words_end, matched_words.end());

    return { matched_words, status };
}



int SearchServer::GetDocumentId(int index) const {
    if (index >= 0 && index < GetDocumentCount()) {
        return *next(document_ids_.begin(), index);
    }

    throw invalid_argument("index out of range");
}

set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (id_word_frequencies_.count(document_id)) {
        return id_word_frequencies_.at(document_id);
    }
    return empty_map;
}


void SearchServer::RemoveDocument(int document_id) {
    return RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    if (!document_ids_.count(document_id)) {
        return;
    }

    for (auto it = word_to_document_freqs_.begin(); it != word_to_document_freqs_.end(); ++it) {
        if (it->second.count(document_id)) {
            it->second.erase(document_id);
        }
    }

    documents_data_.erase(document_id);

    id_word_frequencies_.erase(document_id);

    document_ids_.erase(document_id);    
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    if (!document_ids_.count(document_id)) {
        return;
    }

    document_ids_.erase(document_id);

    documents_data_.erase(document_id);

    const auto& word_freqs = id_word_frequencies_.at(document_id);
    vector<string_view> words(word_freqs.size());

    transform(execution::par, word_freqs.begin(), word_freqs.end(), words.begin(),
        [](const auto& item) {
            return item.first;
        });

    for_each(execution::par, words.begin(), words.end(), 
        [this, document_id](string_view word) {
            word_to_document_freqs_.at(word).erase(document_id);
        });

    id_word_frequencies_.erase(document_id);
}


bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Incorrect word entry");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text.empty()) {
        throw invalid_argument("Empty word"s);
    }

    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }

    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Incorrect word entry or empty word after \"-\" or incorrect word entry after \"-\""s);

    }

    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query query;
    for (const string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}


vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query) const {
    map<int, double> document_to_relevance;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            document_to_relevance[document_id] += term_freq * inverse_document_freq;
        }
    }

    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_data_.at(document_id).rating
            });
    }
    return matched_documents;
}

vector<Document> SearchServer::FindAllDocuments(const Query& query) const {
    return FindAllDocuments(std::execution::seq, query);
}

vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query) const {
    ConcurrentMap<int, double> document_to_relevance(97);

    for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
        [this, &document_to_relevance](string_view word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    );

    for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
        [this, &document_to_relevance](string_view word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
    );


    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_data_.at(document_id).rating });
    }
    return matched_documents;



}



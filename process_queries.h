#pragma once

#include "search_server.h"

//функция ProcessQueries, распараллеливающая обработку нескольких запросов к поисковой системе
std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries);

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries);
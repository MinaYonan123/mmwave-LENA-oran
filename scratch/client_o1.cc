#include "client_o1.hpp"

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    s->append((char*)contents, newLength);
    return newLength;
}

std::string FetchFilteredConfig(const std::string& url,
                                const std::string& filename,
                                const std::string& filterExpr) {
    CURL* curl;
    std::string readBuffer;

    std::string xmlRequest;
    xmlRequest += "<rpc>";
    xmlRequest += "<method>get_config</method>";
    xmlRequest += "<filename>" + filename + "</filename>";
    xmlRequest += "<filter_expr>" + filterExpr + "</filter_expr>";
 
    xmlRequest += "</rpc>";

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist *hs=NULL;
        hs = curl_slist_append(hs, "Content-Type: application/xml");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xmlRequest.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    return readBuffer;
}


std::string FetchConfigXml(const std::string& url, const std::string& filename) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        std::string postData = "<rpc><method>get_config</method><filename>" + filename + "</filename></rpc>";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    return response;
}

bool stob(std::string ip) {
   bool op;
   std::istringstream is(ip); 
   is >> std::boolalpha >> op;
       if (is.fail())
    {
        throw std::invalid_argument(ip.append(" is not convertable to bool"));
    }

   return op;
}

void trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

std::string convertToCondition(const std::string& input) {
    std::string default_name = "cell_name";
    std::string var_name = default_name;
    std::string numbers_part = input;

    if (input.find("==") != std::string::npos) {
        return input;
    }

    
    size_t colon_pos = input.find(':');
    if (colon_pos != std::string::npos) {
        var_name = input.substr(0, colon_pos);
        numbers_part = input.substr(colon_pos + 1);
    
        trim(var_name);
        trim(numbers_part);
        
        if (var_name.empty()) {
            var_name = default_name;
        }
    }

    std::stringstream ss(numbers_part);
    std::string token;
    std::string result;
    bool first = true;
    
    while (std::getline(ss, token, ',')) {
        trim(token);
        if (token.empty()) continue;
        
        if (!first) {
            result += " or ";
        }
        result += var_name + " == " + token;
        first = false;
    }
    
    return result;
}

ConfigData read_config(const std::string& nazwa_pliku) {
    ConfigData config;
    std::ifstream plik(nazwa_pliku);
    
    if (!plik.is_open()) {
        throw std::runtime_error("Cannot open file: " + nazwa_pliku);
    }
    
    if (!std::getline(plik, config.filename)) {
        throw std::runtime_error("File is empty");
    }
    trim(config.filename);
    
    if (!std::getline(plik, config.condition)) {
        throw std::runtime_error("Lack of condition");
    }
    trim(config.condition);
    
    plik.close();
    return config;
};


#include <stdio.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "cJSON.c"

//
// Chroma Client API (https://docs.trychroma.com/js_reference/Client)
//

/**
 * @struct MemoryStruct
 * @brief Structure to store raw data received from an HTTP response.
 * 
 * @var MemoryStruct::memory
 * Pointer to a buffer that stores the response data.
 * @var MemoryStruct::size
 * Length of the data in 'memory'.
 */
typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

/**
 * @struct Collection
 * @brief Represents a collection with its ID and name.
 * 
 * @var Collection::id
 * Identifier for the collection.
 * @var Collection::name
 * Name of the collection.
 */
typedef struct {
    char *id;
    char *name;
    // Handling metadata as a map in C would be more complex
} Collection;

/**
 * @brief Parses a JSON response and extracts collection details.
 * 
 * @param response A string containing the JSON response from the API.
 * @return Collection struct populated with data extracted from the JSON response.
 */
Collection parseCollectionResponse(const char *response) {
    Collection collection = {NULL, NULL};
    
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return collection;
    }

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");

    if (cJSON_IsString(id) && (id->valuestring != NULL)) {
        collection.id = strdup(id->valuestring);
    }

    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        collection.name = strdup(name->valuestring);
    }

    cJSON_Delete(json);
    return collection;
}

/**
 * @brief Frees the memory allocated for a Collection struct.
 * 
 * @param collection A pointer to the Collection struct whose memory needs to be freed.
 */
void freeCollection(Collection *collection) {
    if (collection->id) free(collection->id);
    if (collection->name) free(collection->name);
}

/**
 * @brief Callback function for writing received data into a buffer.
 * 
 * @param contents Pointer to the data received from the server.
 * @param size Size of each data element.
 * @param nmemb Number of data elements.
 * @param userp User-specified pointer (to MemoryStruct) where the received data is stored.
 * @return Total size of data written.
 */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

/**
 * @brief Retrieves collection information from the server.
 * 
 * @param baseUrl Base URL of the API.
 * @param collectionName Name of the collection to retrieve.
 * @return MemoryStruct containing the API response.
 */
MemoryStruct getCollection(const char *baseUrl, const char *collectionName) {
    CURL *curl;
    CURLcode res;
    MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "%s/api/v1/collections/%s", baseUrl, collectionName);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return chunk;
}

/**
 * @brief Tests connectivity with the API server.
 * 
 * @param baseUrl Base URL of the API server.
 * @return 1 if connection is successful, 0 otherwise.
 */
int testConnection(const char *baseUrl) {

    CURL *curl;
    CURLcode res;

    char url[256];
    snprintf(url, sizeof(url), "%s/heartbeat", baseUrl);

    // Initialize CURL 
    curl = curl_easy_init();
    if(curl) {
        // Set the URL for the request
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);

        if(res != CURLE_OK){
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 0;
        }
        else
            printf("HEARTBEAT: Success\n");

        curl_easy_cleanup(curl);
        return 1;
    }
    return 0;
}

/**
 * @brief Creates a new collection on the server.
 * 
 * @param baseUrl Base URL of the API.
 * @param collectionName Name for the new collection.
 * @return 1 on successful creation, 0 on failure.
 */
int createCollection(const char *baseUrl, const char *collectionName) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    char jsonPayload[256];

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    int success = 0;  // Default to failure

    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "%s/api/v1/collections", baseUrl);

        // Prepare JSON payload
        snprintf(jsonPayload, sizeof(jsonPayload), "{\"name\":\"%s\"}", collectionName);

        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set headers for JSON
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set the POST request method
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // Set the payload
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload);

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            success = 1;  // Success
        } else {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    return success;
}

int main() {

    const char *baseUrl = "http://localhost:8000";

    // Test Connection to the chromadb host
    testConnection(baseUrl);
    const char *collectionName = "TestCollection";

    // Create a Collection
    printf("\n\nCreate Collection\n");
    int result = createCollection(baseUrl, collectionName);
    if (result) {
        printf("Collection created successfully.\n");
    } else {
        printf("Failed to create collection.\n");
    }

    // Get Collection
    MemoryStruct response = getCollection(baseUrl, collectionName);
    printf("\n\nGet Collection\n");
    if (response.size > 0) {
        Collection collection = parseCollectionResponse(response.memory);
        if (collection.id != NULL && collection.name != NULL) {
            printf("Collection ID: %s\n", collection.id);
            printf("Collection Name: %s\n", collection.name);
        }
        freeCollection(&collection);
    } else {
        printf("Collection not found or an error occurred.\n");
    }

    free(response.memory);
    return 0;
}

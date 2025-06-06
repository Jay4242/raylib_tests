#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <jansson.h>

// Define the LLM server address
const char* llmServerAddress = "http://localhost:9090";

// Struct to hold the response data
typedef struct {
    char* data;
    size_t size;
} ResponseData;

// Callback function to write the response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    ResponseData* response = (ResponseData*)userp;

    // Reallocate memory to accommodate the new data
    char* newData = realloc(response->data, response->size + totalSize + 1);
    if (!newData) {
        fprintf(stderr, "Memory allocation failed\n");
        return 0; // Indicate an error
    }

    response->data = newData;
    memcpy(&(response->data[response->size]), contents, totalSize);
    response->size += totalSize;
    response->data[response->size] = 0; // Null-terminate the string

    return totalSize;
}

// Function to make a request to the LLM API
char* getLLMResponse(const char* prompt, double temperature) {
    CURL* curl;
    CURLcode res;
    ResponseData response = {NULL, 0};

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        // Construct the URL
        char* url;
        if (asprintf(&url, "%s/v1/chat/completions", llmServerAddress) == -1) {
            fprintf(stderr, "Failed to construct URL\n");
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Prepare the JSON payload
        char* data;
        if (asprintf(&data, "{\"model\": \"llama-3.2-3b-it-q8_0\", \"messages\": [{\"role\": \"system\", \"content\": \"You are a helpful assistant.\"}, {\"role\": \"user\", \"content\": \"%s\"}], \"temperature\": %f}", prompt, temperature) == -1) {
            fprintf(stderr, "Failed to construct JSON payload\n");
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(url);
            return NULL;
        }

        // Set the request type to POST
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

        // Set the data length
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(data));

        // Set the callback function to write the response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Set the content type to application/json
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            curl_global_cleanup();
            free(url);
            return NULL;
        }

        // Always cleanup
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(url);
    } else {
        curl_global_cleanup();
        fprintf(stderr, "curl_easy_init() failed\n");
        return NULL;
    }
    curl_global_cleanup();

    return response.data;
}

// Function to get the theme from the LLM
char** getThemesFromLLM(int* themeCount) {
    char** themes = NULL;
    *themeCount = 0;

    const char* prompt = "Suggest 10 themes for a 'Guess Who?' game. The theme should be who the characters in the game are. For example: clowns, shih-tzu dogs, penguins, llamas, etc. Feel free to be creative and random. Return a JSON list of strings, only the themes and nothing else.";
    double temperature = 1.0;
    char* llmResponse = getLLMResponse(prompt, temperature);

    if (llmResponse != NULL) {
        // Parse the JSON response
        json_error_t error;
        json_t* root = json_loads(llmResponse, 0, &error);

        if (!root) {
            fprintf(stderr, "Error parsing JSON: %s\n", error.text);
            free(llmResponse);
            return NULL;
        }

        json_t* choices = json_object_get(root, "choices");
        if (!json_is_array(choices) || json_array_size(choices) == 0) {
            fprintf(stderr, "Error: 'choices' is not a non-empty array.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        json_t* firstChoice = json_array_get(choices, 0);
        if (!json_is_object(firstChoice)) {
            fprintf(stderr, "Error: First choice is not an object.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        json_t* message = json_object_get(firstChoice, "message");
        if (!json_is_object(message)) {
            fprintf(stderr, "Error: 'message' is not an object.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        json_t* content = json_object_get(message, "content");
        if (!json_is_string(content)) {
            fprintf(stderr, "Error: 'content' is not a string.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        const char* contentStr = json_string_value(content);

        // Strip ```json and ``` from the content string
        char* contentStrStripped = strdup(contentStr);
        if (strncmp(contentStrStripped, "```json", 7) == 0) {
            memmove(contentStrStripped, contentStrStripped + 7, strlen(contentStrStripped) - 6);
        }
        size_t len = strlen(contentStrStripped);
        if (len > 3 && strcmp(contentStrStripped + len - 3, "```") == 0) {
            contentStrStripped[len - 3] = '\0';
        }

        // Parse the content as a JSON array
        json_t* contentArray = json_loads(contentStrStripped, 0, &error);
        if (!json_is_array(contentArray)) {
            fprintf(stderr, "Error parsing content as JSON array: %s\n", error.text);
            json_decref(root);
            free(llmResponse);
            free(contentStrStripped);
            return NULL;
        }

        // Extract the themes from the JSON array
        *themeCount = json_array_size(contentArray);
        themes = (char**)malloc(*themeCount * sizeof(char*));
        if (themes == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            json_decref(root);
            json_decref(contentArray);
            free(llmResponse);
            free(contentStrStripped);
            return NULL;
        }

        for (int i = 0; i < *themeCount; i++) {
            json_t* theme = json_array_get(contentArray, i);
            if (!json_is_string(theme)) {
                fprintf(stderr, "Error: Theme is not a string.\n");
                // Free already allocated themes
                for (int j = 0; j < i; j++) {
                    free(themes[j]);
                }
                free(themes);
                themes = NULL;
                *themeCount = 0;
                json_decref(root);
                json_decref(contentArray);
                free(llmResponse);
                free(contentStrStripped);
                return NULL;
            }
            themes[i] = strdup(json_string_value(theme));
        }

        // Cleanup
        json_decref(root);
        json_decref(contentArray);
        free(llmResponse);
        free(contentStrStripped);
    } else {
        fprintf(stderr, "Failed to get response from LLM\n");
        // If max retries reached, return a default theme
        *themeCount = 1;
        themes = (char**)malloc(sizeof(char*));
        themes[0] = strdup("Default");
    }

    return themes;
}

// Function to get character features from the LLM based on the theme
char** getCharacterFeatures(const char* theme, int* featureCount) {
    char** features = NULL;
    *featureCount = 0;

    // Construct the prompt for the LLM
    char* prompt;
    if (asprintf(&prompt, "Given the theme '%s', suggest 8 distinct features that could be used to differentiate characters in a 'Guess Who?' game. These features should be physical attributes or accessories. Return a JSON list of strings, only the features and nothing else.  For example, if the theme is 'clowns', the features could be: big red nose, blue wig, green hair, top hat, etc.", theme) == -1) {
        fprintf(stderr, "Failed to construct prompt\n");
        return NULL;
    }

    double temperature = 1.0;
    char* llmResponse = getLLMResponse(prompt, temperature);
    free(prompt);

    if (llmResponse != NULL) {
        // Add debug logging: print the raw LLM response
        printf("Raw LLM Response: %s\n", llmResponse);

        // Parse the JSON response
        json_error_t error;
        json_t* root = json_loads(llmResponse, 0, &error);

        if (!root) {
            fprintf(stderr, "Error parsing JSON: %s\n", error.text);
            free(llmResponse);
            return NULL;
        }

        json_t* choices = json_object_get(root, "choices");
        if (!json_is_array(choices) || json_array_size(choices) == 0) {
            fprintf(stderr, "Error: 'choices' is not a non-empty array.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        json_t* firstChoice = json_array_get(choices, 0);
        if (!json_is_object(firstChoice)) {
            fprintf(stderr, "Error: First choice is not an object.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        json_t* message = json_object_get(firstChoice, "message");
        if (!json_is_object(message)) {
            fprintf(stderr, "Error: 'message' is not an object.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        json_t* content = json_object_get(message, "content");
        if (!json_is_string(content)) {
            fprintf(stderr, "Error: 'content' is not a string.\n");
            json_decref(root);
            free(llmResponse);
            return NULL;
        }

        const char* contentStr = json_string_value(content);

        // Add debug logging: print the content string
        printf("Content string: %s\n", contentStr);

        // Strip ```json and ``` from the content string
        char* contentStrStripped = strdup(contentStr);
        if (strncmp(contentStrStripped, "```json", 7) == 0) {
            memmove(contentStrStripped, contentStrStripped + 7, strlen(contentStrStripped) - 6);
        }
        size_t len = strlen(contentStrStripped);
        if (len > 3 && strcmp(contentStrStripped + len - 3, "```") == 0) {
            contentStrStripped[len - 3] = '\0';
        }

        // Parse the content as a JSON array
        json_t* contentArray = json_loads(contentStrStripped, 0, &error);
        if (!json_is_array(contentArray)) {
            fprintf(stderr, "Error parsing content as JSON array: %s\n", error.text);
            json_decref(root);
            free(llmResponse);
            free(contentStrStripped);
            return NULL;
        }

        // Extract the features from the JSON array
        *featureCount = json_array_size(contentArray);
        if (*featureCount != 8) {
            fprintf(stderr, "Warning: Expected 8 features, but got %d\n", *featureCount);
        }

        features = (char**)malloc(*featureCount * sizeof(char*));
        if (features == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            json_decref(root);
            json_decref(contentArray);
            free(llmResponse);
            free(contentStrStripped);
            return NULL;
        }

        for (int i = 0; i < *featureCount; i++) {
            json_t* feature = json_array_get(contentArray, i);
            if (!json_is_string(feature)) {
                fprintf(stderr, "Error: Feature is not a string.\n");
                // Free already allocated features
                for (int j = 0; j < i; j++) {
                    free(features[j]);
                }
                free(features);
                features = NULL;
                *featureCount = 0;
                json_decref(root);
                json_decref(contentArray);
                free(llmResponse);
                free(contentStrStripped);
                return NULL;
            }
            features[i] = strdup(json_string_value(feature));
        }

        // Cleanup
        json_decref(root);
        json_decref(contentArray);
        free(llmResponse);
        free(contentStrStripped);
    } else {
        fprintf(stderr, "Failed to get response from LLM\n");
    }

    return features;
}

// Function to get yes/no input from the user
int getYesNoInput(const char* question) {
    char answer[10];
    while (1) {
        printf("%s (yes/no): ", question);
        if (fgets(answer, sizeof(answer), stdin) == NULL) {
            return 0;
        }
        // Remove trailing newline
        answer[strcspn(answer, "\n")] = 0;
        if (strcmp(answer, "yes") == 0) {
            return 1;
        } else if (strcmp(answer, "no") == 0) {
            return 0;
        } else {
            printf("Invalid input. Please enter 'yes' or 'no'.\n");
        }
    }
}

// Function for the LLM to make a guessing round
void llmGuessingRound(char*** characterTraits, int llmCharacter, const char* theme, int numCharacters, int* charactersRemaining, int* remainingCount) {
    printf("LLM is thinking...\n");

    // Construct the prompt for the LLM to formulate a question
    char* questionPrompt;
    char* characterList = NULL;

    // Build a string containing the character traits, excluding the LLM's own character
    for (int i = 0; i < numCharacters; ++i) {
        if (i == llmCharacter) continue; // Skip the LLM's own character
        // Check if the character is still in the game
        int stillInGame = 0;
        for (int j = 0; j < *remainingCount; j++) {
            if (charactersRemaining[j] == i) {
                stillInGame = 1;
                break;
            }
        }
        if (!stillInGame) continue;

        char* characterString = NULL;
        if (asprintf(&characterString, "Character %d: ", i + 1) == -1) {
            fprintf(stderr, "Failed to construct character string\n");
            return;
        }

        for (int j = 0; j < 2; ++j) { // Assuming each character has 2 features
            if (characterTraits[i] != NULL && characterTraits[i][j] != NULL) {
                char* temp = NULL;
                if (asprintf(&temp, "%s%s%s", characterString, characterTraits[i][j], (j < 1) ? ", " : "") == -1) {
                    fprintf(stderr, "Failed to append feature to character string\n");
                    free(characterString);
                    return;
                }
                free(characterString);
                characterString = temp;
            }
        }

        // Append the character string to the character list
        char* temp2 = NULL;
        if (characterList == NULL) {
            if (asprintf(&temp2, "%s\\n", characterString) == -1) {
                fprintf(stderr, "Failed to construct character list\n");
                free(characterString);
                return;
            }
            characterList = temp2;
        } else {
            if (asprintf(&temp2, "%s%s\\n", characterList, characterString) == -1) {
                fprintf(stderr, "Failed to append character to character list\n");
                free(characterString);
                free(characterList);
                return;
            }
            free(characterList);
            characterList = temp2;
        }
        free(characterString);
    }

    // Construct the question prompt
    if (asprintf(&questionPrompt, "Given the theme '%s' and the following list of characters and their traits: %s Formulate a yes/no question that will help you narrow down the possibilities. The question should be about a single trait from the list of character features. Return the question as a string, only the question and nothing else.", theme, characterList ? characterList : "") == -1) {
        fprintf(stderr, "Failed to construct question prompt\n");
        free(characterList);
        return;
    }

    // Print the prompt before sending it to the LLM
    printf("Prompt sent to LLM:\n%s\n", questionPrompt);

    double temperature = 0.7;
    char* llmQuestionResponse = getLLMResponse(questionPrompt, temperature);
    free(questionPrompt);

    if (llmQuestionResponse != NULL) {
        // Parse the JSON response
        json_error_t error;
        json_t* root = json_loads(llmQuestionResponse, 0, &error);

        if (!root) {
            fprintf(stderr, "Error parsing JSON: %s\n", error.text);
            free(llmQuestionResponse);
            return;
        }

        json_t* choices = json_object_get(root, "choices");
        if (!json_is_array(choices) || json_array_size(choices) == 0) {
            fprintf(stderr, "Error: 'choices' is not a non-empty array.\n");
            fprintf(stderr, "Raw LLM Response: %s\n", llmQuestionResponse); // Print the raw response for debugging
            json_decref(root);
            free(llmQuestionResponse);
            return;
        }

        json_t* firstChoice = json_array_get(choices, 0);
        if (!json_is_object(firstChoice)) {
            fprintf(stderr, "Error: First choice is not an object.\n");
            json_decref(root);
            free(llmQuestionResponse);
            return;
        }

        json_t* message = json_object_get(firstChoice, "message");
        if (!json_is_object(message)) {
            fprintf(stderr, "Error: 'message' is not an object.\n");
            json_decref(root);
            free(llmQuestionResponse);
            return;
        }

        json_t* content = json_object_get(message, "content");
        if (!json_is_string(content)) {
            fprintf(stderr, "Error: 'content' is not a string.\n");
            json_decref(root);
            free(llmQuestionResponse);
            return;
        }

        const char* question = json_string_value(content);

        // Ask the question to the user
        printf("LLM asks: %s\n", question);
        int answer = getYesNoInput("Is this true for your character?");

        // Construct the prompt for the LLM to determine which characters to eliminate
        char* eliminationPrompt;
        const char* answerString = answer ? "yes" : "no";
        if (asprintf(&eliminationPrompt, "Given the theme '%s', the question '%s' was asked, and the answer was '%s'.  Given the following list of characters and their traits: %s Which characters should be eliminated? Return a JSON list of integers, only the character numbers and nothing else.", theme, question, answerString, characterList ? characterList : "") == -1) {
            fprintf(stderr, "Failed to construct elimination prompt\n");
            free(characterList);
            json_decref(root);
            free(llmQuestionResponse);
            return;
        }
        free(characterList);

        printf("Elimination Prompt sent to LLM:\n%s\n", eliminationPrompt);

        char* llmEliminationResponse = getLLMResponse(eliminationPrompt, temperature);
        free(eliminationPrompt);

        if (llmEliminationResponse != NULL) {
            // Add debug logging: Print the raw elimination response
            printf("Raw LLM Elimination Response: %s\n", llmEliminationResponse);

            // Parse the JSON response for the elimination
            json_t* rootElimination = json_loads(llmEliminationResponse, 0, &error);
            if (!rootElimination) {
                fprintf(stderr, "Error parsing JSON for elimination: %s\n", error.text);
                free(llmEliminationResponse);
                return;
            }

            json_t* choicesElimination = json_object_get(rootElimination, "choices");
            if (!json_is_array(choicesElimination) || json_array_size(choicesElimination) == 0) {
                fprintf(stderr, "Error: 'choices' is not a non-empty array for elimination.\n");
                json_decref(rootElimination);
                free(llmEliminationResponse);
                return;
            }

            json_t* firstChoiceElimination = json_array_get(choicesElimination, 0);
            if (!json_is_object(firstChoiceElimination)) {
                fprintf(stderr, "Error: First choice is not an object for elimination.\n");
                json_decref(rootElimination);
                free(llmEliminationResponse);
                return;
            }

            json_t* messageElimination = json_object_get(firstChoiceElimination, "message");
            if (!json_is_object(messageElimination)) {
                fprintf(stderr, "Error: 'message' is not an object for elimination.\n");
                json_decref(rootElimination);
                free(llmEliminationResponse);
                return;
            }

            json_t* contentElimination = json_object_get(messageElimination, "content");
            if (!json_is_string(contentElimination)) {
                fprintf(stderr, "Error: 'content' is not a string for elimination.\n");
                json_decref(rootElimination);
                free(llmEliminationResponse);
                return;
            }

            const char* contentStrElimination = json_string_value(contentElimination);

            // Strip ```json and ``` from the content string
            char* contentStrStrippedElimination = strdup(contentStrElimination);
            if (strncmp(contentStrStrippedElimination, "```json", 7) == 0) {
                memmove(contentStrStrippedElimination, contentStrStrippedElimination + 7, strlen(contentStrStrippedElimination) - 6);
            }
            size_t lenElimination = strlen(contentStrStrippedElimination);
            if (lenElimination > 3 && strcmp(contentStrStrippedElimination + lenElimination - 3, "```") == 0) {
                contentStrStrippedElimination[lenElimination - 3] = '\0';
            }

            // Parse the content as a JSON array
            json_t* contentArrayElimination = json_loads(contentStrStrippedElimination, 0, &error);
            if (!json_is_array(contentArrayElimination)) {
                fprintf(stderr, "Error parsing content as JSON array for elimination: %s\n", error.text);
                json_decref(rootElimination);
                free(llmEliminationResponse);
                free(contentStrStrippedElimination);
                return;
            }

            int numEliminate = json_array_size(contentArrayElimination);
            printf("LLM suggests eliminating %d characters.\n", numEliminate);

            // Iterate through the array and eliminate the characters
            for (int i = 0; i < numEliminate; i++) {
                json_t* charIndex = json_array_get(contentArrayElimination, i);
                if (!json_is_integer(charIndex)) {
                    fprintf(stderr, "Error: Character index is not an integer.\n");
                    continue;
                }
                int characterToEliminate = json_integer_value(charIndex) - 1; // Subtract 1 to get 0-based index

                // Find the character in the remaining characters array and remove it
                int found = 0;
                for (int j = 0; j < *remainingCount; j++) {
                    if (charactersRemaining[j] == characterToEliminate) {
                        // Remove the character by shifting the elements
                        for (int k = j; k < *remainingCount - 1; k++) {
                            charactersRemaining[k] = charactersRemaining[k + 1];
                        }
                        (*remainingCount)--;
                        printf("Eliminating character %d\n", characterToEliminate + 1);
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    fprintf(stderr, "Warning: Character %d not found in remaining characters.\n", characterToEliminate + 1);
                }
            }

            // Cleanup
            json_decref(contentArrayElimination);
            free(contentStrStrippedElimination);
            json_decref(rootElimination);
            free(llmEliminationResponse);
            return;
        } else {
            fprintf(stderr, "Failed to get elimination response from LLM\n");
        }

        // Cleanup
        json_decref(root);
        free(llmQuestionResponse);
    } else {
        fprintf(stderr, "Failed to get response from LLM\n");
    }
}

int main() {
    // Seed the random number generator
    srand(time(NULL));

    // Prompt for theme
    char theme[100];
    printf("Enter a theme for the game (or leave blank for a random theme): ");
    if (fgets(theme, sizeof(theme), stdin) == NULL) {
        return 1;
    }
    // Remove trailing newline
    theme[strcspn(theme, "\n")] = 0;

    // If no theme is entered, get one from the LLM
    if (strlen(theme) == 0) {
        int themeCount;
        char** themes = getThemesFromLLM(&themeCount);
        if (themes != NULL && themeCount > 0) {
            // Generate a random index to select a theme
            int randomIndex = rand() % themeCount;
            strcpy(theme, themes[randomIndex]);
            printf("Using theme suggested by LLM: %s\n", theme);

            // Free the themes array
            for (int i = 0; i < themeCount; i++) {
                free(themes[i]);
            }
            free(themes);
        } else {
            printf("Failed to get theme from LLM, using default theme.\n");
            strcpy(theme, "Default");
        }
    } else {
        printf("Using theme: %s\n", theme);
    }

    // Get character features based on the theme
    int featureCount;
    char** characterFeatures = getCharacterFeatures(theme, &featureCount);

    if (characterFeatures != NULL && featureCount > 0) {
        printf("Character features:\n");
        for (int i = 0; i < featureCount; i++) {
            printf("- %s\n", characterFeatures[i]);
        }

        // Assign 2-3 random features to each of the 24 characters
        int numCharacters = 24;

        // Allocate memory for character traits
        char*** characterTraits = (char***)malloc(numCharacters * sizeof(char**));
        if (characterTraits == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }

        for (int i = 0; i < numCharacters; ++i) {
            int numCharFeatures = 2; // Each character has 2 features
            characterTraits[i] = (char**)malloc(numCharFeatures * sizeof(char*));

            if (characterTraits[i] == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                return 1;
            }

            for (int j = 0; j < numCharFeatures; ++j) {
                int featureIndex = rand() % featureCount;
                characterTraits[i][j] = strdup(characterFeatures[featureIndex]); // Assign the feature to the character
            }
        }

        printf("\nCharacter Traits:\n");
        for (int i = 0; i < numCharacters; ++i) {
            printf("Character %d: ", i + 1);
            int numCharFeatures = 2; // Each character has 2 features
            for (int j = 0; j < numCharFeatures; ++j) {
                printf("%s", characterTraits[i][j]);
                if (j < numCharFeatures - 1) {
                    printf(", ");
                }
            }
            printf("\n");
        }

        // Assign random character to player
        int playerCharacter = rand() % numCharacters;
        printf("\nYou are character number %d\n", playerCharacter + 1);

        // Assign random character to LLM, making sure it's different from the player's
        int llmCharacter;
        do {
            llmCharacter = rand() % numCharacters;
        } while (llmCharacter == playerCharacter);

        // Initialize the array of remaining characters
        int* charactersRemaining = (int*)malloc(numCharacters * sizeof(int));
        if (charactersRemaining == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        int remainingCount = numCharacters;
        for (int i = 0; i < numCharacters; i++) {
            charactersRemaining[i] = i;
        }

        // Game loop
        //int rounds = 5;
        //for (int round = 0; round < rounds; ++round) {
        llmGuessingRound(characterTraits, llmCharacter, theme, numCharacters, charactersRemaining, &remainingCount);
        //}

        // Free memory
        for (int i = 0; i < numCharacters; ++i) {
            int numCharFeatures = 2; // Each character has 2 features
            for (int j = 0; j < numCharFeatures; ++j) {
                free(characterTraits[i][j]);
            }
            free(characterTraits[i]);
        }
        free(characterTraits);

        // Free character features
        for (int i = 0; i < featureCount; i++) {
            free(characterFeatures[i]);
        }
        free(characterFeatures);
    } else {
        printf("No character features found.\n");
    }

    return 0;
}

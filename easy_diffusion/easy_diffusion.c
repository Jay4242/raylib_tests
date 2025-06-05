#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <jansson.h>
#include <curl/curl.h>
#include <raylib.h>

// Configuration
const char* username = "USERNAME";                             //Add username Here.
const char* server_url = "EASY_DIFFUSION_SERVER:PORT";         //Add Easy Diffusion Server:Port here.

// Function to strip the port from the server URL
char* strip_port(const char* url) {
    char* stripped_url = strdup(url);
    char* colon = strchr(stripped_url, ':');
    if (colon != NULL) {
        *colon = '\0';
    }
    return stripped_url;
}

// Base64 decoding table
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Function to generate a random seed
unsigned int get_random_seed() {
    return rand();
}

// Function to execute a shell command and return the output
char* run_command(const char* command) {
    char* result = NULL;
    FILE* pipe = popen(command, "r");
    if (pipe) {
        char buffer[128];
        result = malloc(1); // Start with 1 byte, realloc later
        result[0] = '\0';
        size_t result_len = 0;

        while (!feof(pipe)) {
            size_t bytes_read = fread(buffer, 1, sizeof(buffer), pipe);
            if (bytes_read > 0) {
                result = realloc(result, result_len + bytes_read + 1);
                memcpy(result + result_len, buffer, bytes_read);
                result_len += bytes_read;
                result[result_len] = '\0';
            }
        }
        pclose(pipe);
    }
    return result;
}

// Function to get a random Lora file
char* get_random_lora() {
    char* stripped_url = strip_port(server_url);
    char* command = malloc(512);
    snprintf(command, 512, "ssh %s@%s find /home/%s/Downloads/models/stable-diffusion/lora -type f", username, stripped_url, username);
    free(stripped_url);
    char* lora_files = run_command(command);
    free(command);
    if (!lora_files) return NULL;

    // Count the number of files
    int num_files = 0;
    char* temp = strdup(lora_files);
    char* token = strtok(temp, "\n");
    while (token != NULL) {
        num_files++;
        token = strtok(NULL, "\n");
    }
    free(temp);

    // Choose a random file
    int random_index = rand() % num_files;
    temp = strdup(lora_files);
    char* filename_token = strtok(temp, "\n");
    for (int i = 0; i < random_index; i++) {
        filename_token = strtok(NULL, "\n");
    }

    char* random_lora = strdup(filename_token);
    free(lora_files);
    free(temp);

    // Extract the filename
    char* filename = strrchr(random_lora, '/');
    if (filename) {
        filename++; // Move past the '/'
        char* result = strdup(filename);
        free(random_lora);
        return result;
    } else {
        return random_lora;
    }
}

// Function to get Lora info
char* get_lora_info(const char* lora_file) {
    char command[512];
    char escaped_lora_file[256];
    int j = 0;
    char* stripped_url = strip_port(server_url);
    for (int i = 0; lora_file[i] != '\0' && j < 255; i++) {
        if (lora_file[i] == ' ') {
            escaped_lora_file[j++] = '\\';
        }
        escaped_lora_file[j++] = lora_file[i];
    }
    escaped_lora_file[j] = '\0';

    snprintf(command, sizeof(command), "ssh %s@%s lora_info.bash '/home/%s/Downloads/models/stable-diffusion/lora/%s'", username, stripped_url, username, escaped_lora_file);
    free(stripped_url);
    return run_command(command);
}

// Function to parse Lora info (extract char_line and top_tags)
void parse_lora_info(const char* lora_info, char** char_line, char*** top_tags, int* num_tags) {
    *char_line = NULL;
    *top_tags = NULL;
    *num_tags = 0;

    if (!lora_info) return;

    char* lines[20]; // Assuming a maximum of 20 lines
    int line_count = 0;
    char* temp = strdup(lora_info);
    char* token = strtok(temp, "\n");
    while (token != NULL && line_count < 20) {
        lines[line_count] = strdup(token);
        line_count++;
        token = strtok(NULL, "\n");
    }

    if (line_count >= 3) {
        *char_line = strdup(lines[2]);

        *num_tags = line_count - 3 < 10 ? line_count - 3 : 10;
        *top_tags = malloc(*num_tags * sizeof(char*));
        for (int i = 0; i < *num_tags; i++) {
            (*top_tags)[i] = strdup(lines[3 + i]);
        }
    }

    // Clean up temporary lines
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(temp);
}

// Function to construct the prompt
char* construct_prompt(const char* char_line, char** top_tags, int num_tags) {
    char* prompt = NULL;
    size_t prompt_len = 0;

    if (char_line) {
        prompt_len += strlen(char_line) + 3; // ", " + null terminator
        prompt = malloc(prompt_len);
        snprintf(prompt, prompt_len, "%s, ", char_line);

        if (top_tags && num_tags > 0) {
            for (int i = 0; i < num_tags; i++) {
                prompt_len += strlen(top_tags[i]) + 3; // ", " + null terminator
                prompt = realloc(prompt, prompt_len);
                strcat(prompt, top_tags[i]);
                if (i < num_tags - 1) {
                    strcat(prompt, ", ");
                }
            }
        }
    } else {
        prompt = strdup("");
    }

    return prompt;
}

// Struct to hold the response data
typedef struct {
    char* data;
    size_t size;
} ResponseData;

// Callback function to write the response data
size_t write_callback(char* ptr, size_t size, size_t nmemb, ResponseData* data) {
    size_t new_size = data->size + size * nmemb;
    data->data = realloc(data->data, new_size + 1);
    if (data->data == NULL) {
        printf("realloc() failed\n");
        return 0;
    }
    memcpy(data->data + data->size, ptr, size * nmemb);
    data->data[new_size] = '\0';
    data->size = new_size;
    return size * nmemb;
}

// Function to make an HTTP POST request using libcurl
char* make_http_post(const char* url, const char* data) {
    CURL* curl;
    CURLcode res;
    ResponseData response_data = {NULL, 0};
    struct curl_slist *headers = NULL;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        //printf("POST URL: %s\n", url);
        //printf("POST Data: %s\n", data);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L); // Timeout after 60 seconds

        // Add headers
        headers = curl_slist_append(headers, "Accept: */*");
        headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        headers = curl_slist_append(headers, "Connection: keep-alive");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char origin[256];
        snprintf(origin, sizeof(origin), "Origin: http://%s", server_url);
        headers = curl_slist_append(headers, origin);
        headers = curl_slist_append(headers, "Pragma: no-cache");
        char referer[256];
        snprintf(referer, sizeof(referer), "Referer: http://%s/", server_url);
        headers = curl_slist_append(headers, referer);
        headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Disable SSL verification (equivalent of --insecure)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            if (headers) curl_slist_free_all(headers);
            return NULL;
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    if (headers) curl_slist_free_all(headers);

    return response_data.data;
}

// Function to make an HTTP GET request using libcurl
char* make_http_get(const char* url) {
    CURL* curl;
    CURLcode res;
    ResponseData response_data = {NULL, 0};

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        //printf("GET URL: %s\n", url);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L); // Timeout after 60 seconds

        // Disable SSL verification (equivalent of --insecure)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return NULL;
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    return response_data.data;
}

// Function to decode base64 data
unsigned char* base64_decode(const char* data, size_t data_len, size_t* output_len) {
    size_t i, j;

    if (data_len == 0) {
        *output_len = 0;
        return NULL;
    }

    // Calculate the length of the decoded data
    size_t padding = 0;
    if (data_len > 0 && data[data_len - 1] == '=') padding++;
    if (data_len > 1 && data[data_len - 2] == '=') padding++;
    *output_len = (data_len * 3) / 4 - padding;

    unsigned char* decoded_data = (unsigned char*)malloc(*output_len);
    if (decoded_data == NULL) {
        printf("malloc() failed\n");
        return NULL;
    }

    // Decode the base64 data
    int val = 0, valb = -8;
    for (i = 0, j = 0; i < data_len; i++) {
        unsigned char c = data[i];
        if (c == '=')
            break;

        const char* p = strchr(b64_table, c);
        if (p == NULL)
            continue;

        int index = p - b64_table;
        val = (val << 6) | index;
        valb += 6;

        if (valb >= 0) {
            decoded_data[j++] = (unsigned char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }

    return decoded_data;
}

// Global variables
Texture2D generatedTexture = { 0 };
bool generating = false;
char statusMessage[256] = { 0 };

char* rand_lora = NULL;
char* lora_info = NULL;
char* char_line = NULL;
char** top_tags = NULL;
int num_tags = 0;
char* prompt = NULL;

// Function to unload the generated texture
void unloadGeneratedTexture() {
    if (generatedTexture.id != 0) {
        UnloadTexture(generatedTexture);
        generatedTexture.id = 0;
    }
}

// Function to free Lora data
void freeLoraData() {
    if (rand_lora) free(rand_lora);
    if (lora_info) free(lora_info);
    if (char_line) free(char_line);
    if (top_tags) {
        for (int i = 0; i < num_tags; i++) {
            free(top_tags[i]);
        }
        free(top_tags);
    }
    if (prompt) free(prompt);

    rand_lora = NULL;
    lora_info = NULL;
    char_line = NULL;
    top_tags = NULL;
    num_tags = 0;
    prompt = NULL;
}

// Function to display the final image
int display_final_image(const char* task) {
    char image_url[256];
    snprintf(image_url, sizeof(image_url), "http://%s/image/stream/%s", server_url, task);

    char* final_stream_response = make_http_get(image_url);
    if (!final_stream_response) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to get final stream response from server.");
        printf("Failed to get final stream response from server.\n");
        return 1;
    }

    // Find the start of the base64 image data
    // Find the start of the base64 image data
    char* data_start = strstr(final_stream_response, "\"data\":\"data:image/png;base64,");
    if (!data_start) {
        snprintf(statusMessage, sizeof(statusMessage), "Image data not found in JSON response.");
        printf("Image data not found in JSON response.\n");
        free(final_stream_response);
        return 1;
    }

    // Move the pointer to the beginning of the base64 data
    data_start += strlen("\"data\":\"data:image/png;base64,");

    // Find the end of the base64 data
    char* data_end = strchr(data_start, '"');
    if (!data_end) {
        snprintf(statusMessage, sizeof(statusMessage), "End of image data not found in JSON response.");
        printf("End of image data not found in JSON response.\n");
        free(final_stream_response);
        return 1;
    }

    // Calculate the length of the base64 encoded data
    size_t image_data_len = data_end - data_start;

    // Allocate memory for the base64 encoded data
    char* image_data_base64 = malloc(image_data_len + 1);
    if (!image_data_base64) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to allocate memory for base64 data.");
        printf("Failed to allocate memory for base64 data.\n");
        free(final_stream_response);
        return 1;
    }

    // Copy the base64 encoded data
    strncpy(image_data_base64, data_start, image_data_len);
    image_data_base64[image_data_len] = '\0';

    // Clean up the original response, as it's no longer needed
    free(final_stream_response);

    // Decode the base64 image data
    size_t decoded_size;
    unsigned char* decoded_data = base64_decode(image_data_base64, image_data_len, &decoded_size);
    if (!decoded_data) {
        snprintf(statusMessage, sizeof(statusMessage), "Base64 decoding failed.");
        printf("Base64 decoding failed.\n");
        free(image_data_base64);
        return 1;
    }

    // Free the base64 encoded data
    free(image_data_base64);

    // Load image data into Raylib
    Image image = LoadImageFromMemory(".png", decoded_data, decoded_size);
    if (image.data == NULL) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to load image from memory.");
        printf("Failed to load image from memory.\n");
        free(decoded_data);
        return 1;
    }
    printf("Image loaded from memory successfully, width: %d, height: %d\n", image.width, image.height);

    // Unload the previous texture if it exists
    unloadGeneratedTexture();
    printf("Unloaded generated texture\n");

    generatedTexture = LoadTextureFromImage(image);
    UnloadImage(image);
    free(decoded_data);
    printf("Texture loaded from image, texture ID: %u\n", generatedTexture.id);

    if (generatedTexture.id == 0) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to load texture from image.");
        fprintf(stderr, "Failed to load texture from image.\n");
        return 1;
    }

    snprintf(statusMessage, sizeof(statusMessage), "Image generated successfully!");
    return 0;
}

// Function to generate the image using libcurl
int generate_image(const char* prompt, const char* rand_lora) {
    printf("Starting image generation with prompt: %s, lora: %s\n", prompt, rand_lora);
    char render_url[256];
    snprintf(render_url, sizeof(render_url), "http://%s/render", server_url);
    char* data = malloc(4096);
    if (!data) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to allocate memory for data.");
        return 1;
    }
    int snprintf_result = snprintf(data, 4096,
                                  "{"
                                  "\"prompt\": \"%s\", "
                                  "\"seed\": %u, "
                                  "\"used_random_seed\": true, "
                                  "\"negative_prompt\": \"\", "
                                  "\"num_outputs\": 1, "
                                  "\"num_inference_steps\": 30, "
                                  "\"guidance_scale\": 7.5, "
                                  "\"width\": 512, "
                                  "\"height\": 768, "
                                  "\"vram_usage_level\": \"balanced\", "
                                  "\"sampler_name\": \"dpmpp_3m_sde\", "
                                  "\"use_stable_diffusion_model\": \"absolutereality_v181\", "
                                  "\"clip_skip\": true, "
                                  "\"use_vae_model\": \"\", "
                                  "\"stream_progress_updates\": true, "
                                  "\"stream_image_progress\": false, "
                                  "\"show_only_filtered_image\": true, "
                                  "\"block_nsfw\": false, "
                                  "\"output_format\": \"png\", "
                                  "\"output_quality\": 75, "
                                  "\"output_lossless\": false, "
                                  "\"metadata_output_format\": \"embed,json\", "
                                  "\"original_prompt\": \"%s\", "
                                  "\"active_tags\": [], "
                                  "\"inactive_tags\": [], "
                                  "\"save_to_disk_path\": \"/home/%s/Pictures/stable-diffusion/output/\", "
                                  "\"use_lora_model\": [\"%s\"], "
                                  "\"lora_alpha\": [\"0.7\"], "
                                  "\"enable_vae_tiling\": false, "
                                  "\"scheduler_name\": \"automatic\", "
                                  "\"session_id\": \"1337\""
                                  "} ",
                                  prompt, get_random_seed(), prompt, username, rand_lora);
    if (snprintf_result < 0 || snprintf_result >= 4096) {
        snprintf(statusMessage, sizeof(statusMessage), "Error creating data string (truncation detected) %d.", snprintf_result);
        free(data);
        return 1;
    }

    char* response = make_http_post(render_url, data);
    if (!response) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to get response from server.");
        free(data);
        return 1;
    }

    // Parse the JSON response to get the task ID
    json_error_t error;
    json_t* root = json_loads(response, 0, &error);
    if (!root) {
        snprintf(statusMessage, sizeof(statusMessage), "Error parsing JSON: %s", error.text);
        free(response);
        return 1;
    }

    json_t* task_json = json_object_get(root, "task");
    const char* task = NULL;
    char* task_str = NULL; // Declare task_str here
    if (!task_json) {
        snprintf(statusMessage, sizeof(statusMessage), "Task ID not found in JSON response.");
        // Check for status
        json_t* status_json = json_object_get(root, "status");
        if (status_json && json_is_string(status_json)) {
            const char* status_str = json_string_value(status_json);
            if (strcmp(status_str, "succeeded") == 0) {
                printf("Task succeeded, attempting to fetch final image.\n");
                // Fetch the final image directly
                json_decref(root);
                free(response);
                char fake_task[] = "1337";
                int result = display_final_image(fake_task);
                return result;
            } else {
                printf("Status: %s\n", status_str);
            }
        }
        json_decref(root);
        free(response);
        free(data);
        return 1;
    }

    if (json_is_integer(task_json)) {
        // Convert the integer task ID to a string
        long long task_id = json_integer_value(task_json);
        task_str = malloc(32); // Allocate memory for the string
        if (!task_str) {
            snprintf(statusMessage, sizeof(statusMessage), "Failed to allocate memory for task ID.");
            json_decref(root);
            free(response);
            free(data);
            return 1;
        }
        snprintf(task_str, 32, "%lld", task_id);
        task = task_str; // Assign the allocated string to task
    } else if (json_is_string(task_json)) {
        task = json_string_value(task_json);
        if (!task) {
            snprintf(statusMessage, sizeof(statusMessage), "Task ID is not a string.");
            json_decref(root);
            free(response);
            free(data);
            return 1;
        }
    } else {
        snprintf(statusMessage, sizeof(statusMessage), "Task ID is not a string or integer.");
        json_decref(root);
        free(response);
        free(data);
        return 1;
    }

    printf("Task ID: %s\n", task);
    json_decref(root);
    free(response);
    free(data);

    // Poll for the image
    char image_url[256];
    char ping_url[256];
    snprintf(image_url, sizeof(image_url), "http://%s/image/stream/%s", server_url, task);
    snprintf(ping_url, sizeof(ping_url), "http://%s/ping?session_id=1337", server_url);

    char* status = strdup("pending");
    char percent[8] = "0%";
    int result = 1;

    while (strcmp(status, "completed") != 0) {
        if (strcmp(status, "error") == 0) {
            snprintf(statusMessage, sizeof(statusMessage), "Error occurred during rendering.");
            fprintf(stderr, "Error occurred during rendering.\n");
            if (task_str) free(task_str);
            return 1;
        }

        if (strcmp(status, "Online") == 0) {
            printf("Status is Online, generation complete.\n");
            result = 0;
            break;
        }

        sleep(5);

        // Get the status
        char* ping_response = make_http_get(ping_url);
        if (!ping_response) {
            snprintf(statusMessage, sizeof(statusMessage), "Failed to get ping response from server.");
            printf("Failed to get ping response from server.\n");
            if (task_str) free(task_str);
            free(status);
            return 1;
        }
        //printf("Ping Response: %s\n", ping_response);

        // Debug: Print the raw ping response
        //printf("Raw Ping Response: %s\n", ping_response);

        // Extract the status string
        char* status_start = strstr(ping_response, "\"status\":\"");
        if (status_start != NULL) {
            status_start += strlen("\"status\":\"");
            char* status_end = strchr(status_start, '"');
            if (status_end != NULL) {
                // Calculate the length of the status string
                size_t status_len = status_end - status_start;

                // Allocate memory for the status string
                char* extracted_status = (char*)malloc(status_len + 1);
                if (extracted_status == NULL) {
                    printf("Failed to allocate memory for status.\n");
                    free(ping_response);
                    if (task_str) free(task_str);
                    free(status);
                    return 1;
                }

                // Copy the status string
                strncpy(extracted_status, status_start, status_len);
                extracted_status[status_len] = '\0';

                // Use the extracted status
                free(status);
                status = extracted_status;
                printf("Extracted Status: %s\n", status);
            } else {
                printf("Could not find end quote for status.\n");
            }
        } else {
            printf("Could not find status field in ping response.\n");
        }
        free(ping_response);

        // Get the stream data
        char* stream_response = make_http_get(image_url);
        if (!stream_response) {
            snprintf(statusMessage, sizeof(statusMessage), "Failed to get stream response from server.");
            printf("Failed to get stream response from server.\n");
            if (task_str) free(task_str);
            free(status);
            return 1;
        }
        //printf("Stream Response: %s\n", stream_response);

        // Debug: Print the raw stream response
        //printf("Raw Stream Response: %s\n", stream_response);

        // Split the stream response into individual JSON objects
        char* stream_data_str = strtok(stream_response, "}{");
        while (stream_data_str != NULL) {
            // Add back the missing brackets
            char json_string[2048];
            snprintf(json_string, sizeof(json_string), "%s%s%s",
                     stream_data_str[0] == '{' ? "" : "{",
                     stream_data_str,
                     stream_data_str[strlen(stream_data_str) - 1] == '}' ? "" : "}");

            // Process the stream data as a single JSON object
            json_error_t stream_error;
            json_t* stream_data = json_loads(json_string, 0, &stream_error);
            if (!stream_data) {
                //printf("Error parsing JSON: %s\n", stream_error.text);
                //printf("Response content: %s\n", json_string);
                stream_data_str = strtok(NULL, "}{");
                continue;
            }

            json_t* steps_json = json_object_get(stream_data, "step");
            json_t* total_steps_json = json_object_get(stream_data, "total_steps");

            if (steps_json && total_steps_json && json_is_number(steps_json) && json_is_number(total_steps_json)) {
                int steps = json_integer_value(steps_json);
                int total_steps = json_integer_value(total_steps_json);
                snprintf(percent, sizeof(percent), "%d%%", (int)((float)steps / total_steps * 100));
            }

            printf("Task Status: %s, Task: %s, Prompt: %s, Percent Done: %s\n",
                   status, task, prompt, percent);

            json_decref(stream_data);
            stream_data_str = strtok(NULL, "}{");
        }
        free(stream_response);

        printf("Task Status: %s, Task: %s, Prompt: %s, Percent Done: %s\n",
               status, task, prompt, percent);
    }

    // Get the final image
    int final_result = (result == 0) ? display_final_image(task) : 1;
    if (task_str) free(task_str); // Free the allocated memory for task
    free(status);
    return final_result;
}

// Function to load Lora data and construct the prompt
int loadLoraData() {
    freeLoraData();

    rand_lora = get_random_lora();
    if (!rand_lora) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to retrieve a random Lora file.");
        printf("Failed to retrieve a random Lora file.\n");
        return 1;
    }
    printf("Random Lora: %s\n", rand_lora);

    lora_info = get_lora_info(rand_lora);
    if (!lora_info) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to retrieve Lora info.");
        printf("Failed to retrieve Lora info.\n");
        return 1;
    }
    printf("Lora Info:\n%s\n", lora_info);

    parse_lora_info(lora_info, &char_line, &top_tags, &num_tags);

    prompt = construct_prompt(char_line, top_tags, num_tags);
    if (!prompt) {
        snprintf(statusMessage, sizeof(statusMessage), "Failed to construct the prompt.");
        printf("Failed to construct the prompt.\n");
        return 1;
    }
    printf("Prompt: %s\n", prompt);

    return 0;
}

int main(void) {
    // Seed the random number generator
    srand(time(NULL));

    // Load initial Lora data
    if (loadLoraData() != 0) {
        CloseWindow();
        return 1;
    }

    // Raylib window initialization
    const int screenWidth = 800;
    const int screenHeight = 800;
    InitWindow(screenWidth, screenHeight, "Easy Diffusion in C");
    SetTargetFPS(60);

    // Define button rectangles
    Rectangle generateButton = { 10, 10, 120, 30 };
    Rectangle newLoraButton = { 10, 50, 120, 30 };

    // Image display area
    Rectangle imageDisplayArea = { (screenWidth - 512) / 2, 90, 512, 768 };

    // Main game loop
    while (!WindowShouldClose()) {
        // Check if the generate button is clicked
        if (CheckCollisionPointRec(GetMousePosition(), generateButton) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !generating) {
            generating = true;
            snprintf(statusMessage, sizeof(statusMessage), "Generating image...");
            unloadGeneratedTexture();

            // Launch image generation in a separate thread
            int result = 1;
            if (prompt && rand_lora) {
                result = generate_image(prompt, rand_lora);
                if (result == 0) {
                    printf("Image generation initiated.\n");
                } else {
                    printf("Image generation failed.\n");
                }
            } else {
                snprintf(statusMessage, sizeof(statusMessage), "Prompt or Lora data is not initialized.");
                printf("Prompt or Lora data is not initialized.\n");
            }
            generating = false;
        }

        // Check if the new Lora button is clicked
        if (CheckCollisionPointRec(GetMousePosition(), newLoraButton) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !generating) {
            snprintf(statusMessage, sizeof(statusMessage), "Loading new Lora...");
            if (loadLoraData() != 0) {
                printf("Failed to load new Lora data.\n");
            }
        }

        // Drawing
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw the generate button
        DrawRectangleRec(generateButton, SKYBLUE);
        DrawText("Generate", generateButton.x + 10, generateButton.y + 10, 20, BLACK);

        // Draw the new Lora button
        DrawRectangleRec(newLoraButton, VIOLET);
        DrawText("New Lora", newLoraButton.x + 10, newLoraButton.y + 10, 20, BLACK);

        // Draw the generated image if it exists
        if (generatedTexture.id != 0) {
            DrawTexturePro(
                generatedTexture,
                (Rectangle){ 0, 0, generatedTexture.width, generatedTexture.height }, // Source rectangle
                imageDisplayArea,       // Destination rectangle
                (Vector2){ 0, 0 },          // Origin (top-left corner)
                0.0f,                       // Rotation
                WHITE                       // Tint
            );
        }

        // Draw Status Message
        DrawText(statusMessage, 10, screenHeight - 25, 20, GRAY);

        EndDrawing();
    } // Close the main game loop

    // De-Initialization
    unloadGeneratedTexture();
    freeLoraData();

    CloseWindow();
    return 0;
}

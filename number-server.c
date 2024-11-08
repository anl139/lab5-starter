#include "http-server.h"
#include <string.h>

int num = 0; // the state of the server (the hello world version of chats list
#define USERNAME_SIZE 15
#define MESSAGE_SIZE 255
#define TIMESTAMP_SIZE 20
#define MAX_REACTIONS 10
#define REACTION_SIZE 100
#define REACTION_MESSAGE_SIZE 15

// Assuming there's an array or linked list of chats:
#define MAX_CHATS 100
char const HTTP_404_NOT_FOUND[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_200_OK[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_400_BAD_REQUEST[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\n";
typedef struct {
    char user[USERNAME_SIZE+1];
    char message[REACTION_MESSAGE_SIZE+1];  // Example reaction types like "like" or "love"
} Reaction;

typedef struct {
    uint32_t id;
    char user[USERNAME_SIZE+1];
    char message[MESSAGE_SIZE+1];
    char timestamp[TIMESTAMP_SIZE];
    uint32_t num_reactions; 
    Reaction reactions[MAX_REACTIONS];
} Chat;


Chat *chat_list[MAX_CHATS] = {NULL};
uint32_t current_id = 1;  // Initialize current_id as 1
int chat_count = 0;


void handle_400(int client_sock, const char *error_msg) {
    char response_buff[BUFFER_SIZE];
    snprintf(response_buff, BUFFER_SIZE, "Error 400: %s\n", error_msg);
    write(client_sock, HTTP_400_BAD_REQUEST, strlen(HTTP_400_BAD_REQUEST));
    write(client_sock, response_buff, strlen(response_buff));
}

void get_current_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

// Function to add a new chat
uint8_t add_chat(char *username, char *message) {
    if (chat_count >= MAX_CHATS) {
        return 0;  // Exceeds maximum number of chats
    }

    if (strlen(username) > USERNAME_SIZE || strlen(message) > MESSAGE_SIZE) {
        return 0;  // Exceeds max length for username or message
    }

    // Allocate memory for the new chat
    Chat *new_chat = (Chat *)malloc(sizeof(Chat));
    if (new_chat == NULL) {
        return 0;  // Allocation failed
    }

    // Set the chat ID and update current_id
    new_chat->id = current_id++;
    
    // Copy username and message to new chat
    strncpy(new_chat->user, username, USERNAME_SIZE);
    new_chat->user[USERNAME_SIZE] = '\0';
    strncpy(new_chat->message, message, MESSAGE_SIZE);
    new_chat->message[MESSAGE_SIZE] = '\0';

    // Set the current timestamp
    get_current_timestamp(new_chat->timestamp, TIMESTAMP_SIZE);

    // Initialize reaction count to 0
    new_chat->num_reactions = 0;

    // Add the new chat to the chat list
    chat_list[chat_count++] = new_chat;

    return 1;
}
uint8_t add_reaction(char* username, char* reaction_message, char* id) {
	uint32_t chat_id = atoi(id); // Convert the id string to an integer
	char response_buff[BUFFER_SIZE];
    if (chat_id == 0 || chat_id > chat_count) {
	    snprintf(response_buff, BUFFER_SIZE, "Error 1: \n");
        return 0;  // Error: Invalid ID
    }
   if (strlen(username) > USERNAME_SIZE || strlen(reaction_message) > REACTION_MESSAGE_SIZE){
	   snprintf(response_buff, BUFFER_SIZE, "Error2:\n");
	   return 0;
   }

    if (chat_list[chat_id - 1] == NULL){
	    snprintf(response_buff, BUFFER_SIZE, "Error 3:\n");
	    return 0;
    }
    // Check if num_reactions has reached the maximum allowed
    // Add the reaction directly in chats_list
    Reaction *new_reaction = &chat_list[chat_id - 1]->reactions[chat_list[chat_id - 1]->num_reactions];
    strncpy(new_reaction->user, username, USERNAME_SIZE);
    new_reaction->user[USERNAME_SIZE] = '\0';  // Ensure null termination
    strncpy(new_reaction->message, reaction_message, 15);
    new_reaction->message[15] = '\0';   // Ensure null termination

    // Increment num_reactions directly in chats_list
    chat_list[chat_id - 1]->num_reactions++;
    printf("Reaction added to chat %u by user %s.\n", chat_id, username);
    return 1;  // Success
}
void handle_reset(int client) {
	int i;
	int j;
    // Free each chat and its associated reactions
    for (i = 0; i < chat_count; i++) {
        free(chat_list[i]);  // Free each chat
    }

    // Reset global chat count and reaction counters
    chat_count = 0;

    // Respond with an HTTP success message and an empty body
    write(client, HTTP_200_OK, strlen(HTTP_200_OK));
}

void url_decode(char *str) {
    char *pstr = str;
    char ch;
    int i;
    while (*str) {
        if (*str == '%') {
            // Convert %xx to character
            if (sscanf(str + 1, "%2x", &i) == 1) {
                ch = (char)i;
                *pstr++ = ch;
                str += 3;  // Skip the "%" and the two hex digits
            }
            else {
                *pstr++ = *str++;  // If not a valid hex sequence, just copy it
            }
        } else {
            *pstr++ = *str++;  // Just copy the character
        }
    }
    *pstr = '\0';  // Null-terminate the string
}

int get_query_param(const char *query, const char *param_name, char *out_value, size_t max_len) {
    char search_key[50];
    snprintf(search_key, sizeof(search_key), "%s=", param_name);  // Format the key to look for "param_name="

    const char *start = strstr(query, search_key);  // Find the position of "param_name="
    if (!start) {
        return 0;  // Parameter not found
    }

    start += strlen(search_key);  // Move start to the beginning of the value
    const char *end = strchr(start, '&');  // Find the end of the parameter (or end of the query string)

    size_t value_len = (end) ? (size_t)(end - start) : strlen(start);  // Calculate length of value
    if (value_len >= max_len) {  // Ensure it fits in the buffer, including null-terminator
        return 0;
    }

    strncpy(out_value, start, value_len);  // Copy value into output buffer
    out_value[value_len] = '\0';  // Null-terminate the output string
    url_decode(out_value);
    return 1;
}
void handle_chats(int client_sock) {
    char response[BUFFER_SIZE] = "";  // Initialize an empty response buffer
    char temp[BUFFER_SIZE];           // Temporary buffer to format each chat line

    // Iterate over each chat in chat_list and format the output
    for (int i = 0; i < MAX_CHATS && chat_list[i] != NULL; i++) {
        Chat *chat = chat_list[i];

        // Format the chat message
        snprintf(temp, sizeof(temp), "[#%u %s] %s: %s\n",
                 chat->id, chat->timestamp, chat->user, chat->message);

        // Ensure there's enough space before appending to the response buffer
        if (strlen(response) + strlen(temp) < sizeof(response)) {
            strncat(response, temp, sizeof(response) - strlen(response) - 1);
        }

        // Add each reaction to the response with specific formatting
        for (int j = 0; j < chat->num_reactions; j++) {
            Reaction *reaction = &chat->reactions[j];
            snprintf(temp, sizeof(temp), "                    (%s)  %s\n",
                     reaction->user, reaction->message);
            if (strlen(response) + strlen(temp) < sizeof(response)) {
                strncat(response, temp, sizeof(response) - strlen(response) - 1);
            }
        }
    }

    // Send the response back to the client
    write(client_sock, HTTP_200_OK, strlen(HTTP_200_OK));
    write(client_sock, response, strlen(response));
}
void handle_post(int client_sock, const char *query) {
    char username[USERNAME_SIZE + 1] = {0};
    char message[MESSAGE_SIZE + 1] = {0};

    // Use get_query_param to parse the "user" and "message" parameters
    if (!get_query_param(query, "user", username, USERNAME_SIZE + 1) ||
        !get_query_param(query, "message", message, MESSAGE_SIZE + 1)) {
        handle_400(client_sock, "Missing 'user' or 'message' parameter.");
        return;
    }

    if (!add_chat(username, message)) {
        handle_400(client_sock, "Unable to add chat: max chats reached or invalid data.");
        return;
    }

    // Respond with success
    handle_chats(client_sock);
    
}
void handle_reaction(int client_sock, const char *query) {
    char username[USERNAME_SIZE + 1] = {0};
    char message[MESSAGE_SIZE + 1] = {0};
    char id_str[4] = {0};
    int8_t chat_id;
     if (!get_query_param(query, "user", username, sizeof(username)) ||
        !get_query_param(query, "message", message, sizeof(message)) ||
        !get_query_param(query, "id", id_str, sizeof(id_str))) {
        write(client_sock, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing 'user', 'message', or 'id' parameter.", 89);
        return;
    }

    // Convert id_str to an integer
    chat_id = atoi(id_str);
    if (chat_id <= 0) {
        write(client_sock, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid 'id' parameter.", 67);
        return;
    }

    // Add the reaction to the specified chat
    if (!add_reaction(username, message, id_str)) {
        handle_400(client_sock, "Unable to add reaction: max reactions reached or invalid data.");
        return;
    }

    // Respond with the updated chat list
    handle_chats(client_sock);
}

void handle_404(int client_sock, char *path)  {
    printf("SERVER LOG: Got request for unrecognized path \"%s\"\n", path);

    char response_buff[BUFFER_SIZE];
    snprintf(response_buff, BUFFER_SIZE, "Error 404:\r\nUnrecognized path \"%s\"\r\n", path);
    // snprintf includes a null-terminator

    // TODO: send response back to client?
    write(client_sock, HTTP_404_NOT_FOUND, strlen(HTTP_404_NOT_FOUND));
    write(client_sock, response_buff, strlen(response_buff));
}

void handle_root(int client_sock) {
  char message[BUFFER_SIZE];
  snprintf(message, BUFFER_SIZE, "Current number: %d\n", num);
  write(client_sock, HTTP_200_OK, strlen(HTTP_200_OK));  
  write(client_sock, message, strlen(message));
}

void handle_response(char *request, int client_sock) {
    char path[256];
    char *query = NULL;

    printf("\nSERVER LOG: Got request: \"%s\"\n", request);

    // Parse the path out of the request line (limit buffer size; sscanf null-terminates)
    if (sscanf(request, "GET %255s", path) != 1) {
        printf("Invalid request line\n");
        return;
    }
     query = strchr(path, '?');
    if (query) {
        *query++ = '\0';  // Null-terminate path and set query to parameters
    }
    // How to write the if statements to detect which path we have?
    if(strcmp(path, "/") == 0) {
      handle_root(client_sock);
      return;
    }
    else if(strcmp(path,"/post")== 0 && query){
	    handle_post(client_sock,query);
	    return;
    }else if(strcmp(path,"/chats") == 0){
	   handle_chats(client_sock);
	   return;
    }
     else if(strcmp(path,"/react")== 0){
	     handle_reaction(client_sock,query);
	     return;
    }else if(strcmp(path,"/reset") == 0){
	   handle_reset(client_sock);
    } else {
      handle_404(client_sock, path);
    }

    // strstr if there might be shared prefixes, like looking for "/post" in the PA
    // save strstr for later

}

int main(int argc, char *argv[]) {
   int port = 0;
   if(argc >= 2) { // if called with a port number, use that
        port = atoi(argv[1]);
    }
   start_server(&handle_response, port);  
 }

#include "http-server.h"
#include <string.h>

int num = 0; // the state of the server (the hello world version of chats list
#define USERNAME_SIZE 50
#define MESSAGE_SIZE 200
#define TIMESTAMP_SIZE 20
#define MAX_REACTIONS 10
#define REACTION_SIZE 200

// Assuming there's an array or linked list of chats:
#define MAX_CHATS 100
char const HTTP_404_NOT_FOUND[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_200_OK[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
char const HTTP_400_BAD_REQUEST[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\n";
typedef struct {
    char user[USERNAME_SIZE + 1];
    char message[15];  // Example reaction types like "like" or "love"
} Reaction;

typedef struct {
    uint32_t id;
    char user[USERNAME_SIZE+ 1];
    char message[MESSAGE_SIZE + 1];
    char timestamp[TIMESTAMP_SIZE];
    uint32_t num_reactions; 
    Reaction reactions[MAX_REACTIONS];
} Chat;

Chat *chat_list[MAX_CHATS] = {NULL};
uint32_t current_id = 1;  // Initialize current_id as 1
int chat_count = 0;
int reaction_count = 0;
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
    if (chat_id == 0 || chat_id > chat_count) {
        printf("Error: Invalid chat ID %u.\n", chat_id);
        return 1;  // Error: Invalid ID
    }

   if (strlen(username) > USERNAME_SIZE || strlen(reaction_message) > 15){
	   return 1;
   }

    if (chat_list[chat_id - 1] == NULL){
	    return 1;
    }
    // Check if num_reactions has reached the maximum allowed
    // Add the reaction directly in chats_list
    Reaction *new_reaction = &chat_list[chat_id - 1]->reactions[0];
    strncpy(new_reaction->user, username, USERNAME_SIZE);
    new_reaction->user[USERNAME_SIZE] = '\0';  // Ensure null termination
    strncpy(new_reaction->message, reaction_message, 15);
    new_reaction->message[15] = '\0';   // Ensure null termination

    // Increment num_reactions directly in chats_list
    chat_list[chat_id - 1]->num_reactions++;
    printf("Reaction added to chat %u by user %s.\n", chat_id, username);
    return 0;  // Success
}
void reset() {
    // Free dynamically allocated memory if needed (currently no dynamic allocation for reactions)
    int i;
   for (i = 0; i < MAX_CHATS; i++) {
        if (chat_list[i] != NULL) {
            free(chat_list[i]);  // Free the memory allocated for the chat
            chat_list[i] = NULL;  // Reset the pointer to NULL
        }
    }

    // Optionally reset the chat count or any other relevant variables
    chat_count = 0;
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
    return 1;
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
    char response_buff[BUFFER_SIZE];
    snprintf(response_buff, BUFFER_SIZE, "Chat by %s added: %s\n", username, message);
    write(client_sock, HTTP_200_OK, strlen(HTTP_200_OK));
    write(client_sock, response_buff, strlen(response_buff));
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

void handle_increment(int client_sock) {
  char message[BUFFER_SIZE];
  num += 1;
  snprintf(message, BUFFER_SIZE, "Incremented to: %d\n", num);
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
    /* "/" – shows “Current number: ____”
        "/increment" – adds 1 to number and shows “Incremented to: _____” */
    // How to write the if statements to detect which path we have?
    if(strcmp(path, "/") == 0) {
      handle_root(client_sock);
      return;
    }
    else if(strcmp(path, "/increment") == 0) {
      handle_increment(client_sock);
      return;
    }
    else if(strcmp(path,"/post")== 0 && query){
	    return;
    } else {
      handle_404(client_sock, path);
    }

    // strstr if there might be shared prefixes, like looking for "/post" in the PA
    // save strstr for later

}

int main(int argc, char *argv[]) {
   // int port = 0;
   // if(argc >= 2) { // if called with a port number, use that
      //  port = atoi(argv[1]);
   // }

   // start_server(&handle_response, port);
    add_chat("alice", "Hello world");
    add_reaction("bob", "nice", "1"); 
 if (chat_list[0]->id != 0) {  // Assuming an id of 0 means the chat hasn't been added yet
        printf("First chat in list:\n");
        printf("ID: %u\n", chat_list[0]->id);
        printf("User: %s\n", chat_list[0]->user);
        printf("Message: %s\n", chat_list[0]->message);
        printf("Timestamp: %s\n", chat_list[0]->timestamp);
	printf("Number of Reactions: %d\n", chat_list[0]->num_reactions);
	printf("Reaction: %s", chat_list[0]->reactions[0].user);
	printf(" %s", chat_list[0]->reactions[0].message);
    }}

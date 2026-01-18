#ifndef ABBY_CLIENT_H
#define ABBY_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle
typedef void* AbbyClientHandle;

// Lifecycle
AbbyClientHandle abby_client_create(void);
void abby_client_destroy(AbbyClientHandle client);

// Connection
int abby_client_connect(AbbyClientHandle client);
void abby_client_disconnect(AbbyClientHandle client);
int abby_client_is_connected(AbbyClientHandle client);

// Commands (returns newly allocated string, caller must free)
char* abby_client_send_command(AbbyClientHandle client, const char* cmd);
char* abby_client_get_status(AbbyClientHandle client);

// Playback
int abby_client_play(AbbyClientHandle client, const char* filepath);
int abby_client_stop(AbbyClientHandle client);

// Visuals
int abby_client_start_visuals(AbbyClientHandle client);
int abby_client_stop_visuals(AbbyClientHandle client);
int abby_client_next_shader(AbbyClientHandle client);
int abby_client_prev_shader(AbbyClientHandle client);

#ifdef __cplusplus
}
#endif

#endif // ABBY_CLIENT_H

#pragma once

int client_open(char *command, int sock_fd);

int client_ls(int sock_fd);

int client_get(char *command, int sock_fd);

int client_put(char *command, int sock_fd);

int client_sha256(char *command, int sock_fd);

int client_quit(int sock_fd);
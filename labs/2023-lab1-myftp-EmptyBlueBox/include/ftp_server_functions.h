#pragma once

int server_open(int sock_fd);

int server_ls(int sock_fd);

int server_get(char *file_name, int sock_fd);

int server_put(char *file_name, int sock_fd);

int server_sha256(char *file_name, int sock_fd);

int server_quit(int sock_fd);
#include "parent.h"
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <time.h>
#include <stdio.h>
#include "child.h"
#include "utils.h"
#include "config.h"

static server_item *children;
int used_children = 0;
int server_socket;

void sigchld_handler(int sig);

void init_server(){
    // setting SIGCHLD handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);

    // allocating shared memory
    children = mmap(NULL, sizeof (server_item) * (MAX_CHILD_COUNT + 1),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    int i = 0;
    for(; i < MAX_CHILD_COUNT; i++){
        children[i].pid = 0;
        children[i].state = SERVER_ITEM_DEAD;
    }

    config *config = config_get();
    bind_server(config);

    int base_fork = config->min_children;
    if (base_fork < 1 || base_fork > MAX_CHILD_COUNT){
        die_with_error("min_children must be between 1 and MAX_CHILD_COUNT");
    }

    for (i = 0; i < base_fork; i++){
        fork_child(children + i);
    }

    used_children = base_fork;
}

void bind_server(config *conf){
    struct sockaddr_in addr;
    if (str_to_sockaddr_ipv4(conf->bind_to, &addr) == 0){
        die_with_error("bind: parse failed");
    }
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        die_with_error("socket failed");
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(server_socket, (struct sockaddr *) &addr,
          sizeof(addr)) < 0) {
        die_with_error("bind failed");
    }

    printf("Listening on %s..\n", conf->bind_to);

    listen(server_socket,512);
}

void fork_child(server_item *item){
    if (item->state != SERVER_ITEM_DEAD){
        return;
    }

    item->state = SERVER_ITEM_AVAILABLE;

    pid_t pid = fork();
    if (pid < 0){
        die_with_error("Fork failed");
    }
    else if (pid > 0){
        item->pid = pid;
    }
    else if (pid == 0){
        process_client(server_socket, item);
        exit(0);
    }
}

void check_children(){
    config *config = config_get();
    int alive_count = 0, available_count = 0, i = 0;

    for(; i < used_children; i++){
        if (children[i].state == SERVER_ITEM_DEAD){
            continue;
        }

        alive_count++;
        if (children[i].state == SERVER_ITEM_AVAILABLE){
            available_count ++;
        }
    }

    int add_count = 0;
    if (alive_count < config->min_children){
        add_count = config->min_children - alive_count;
    }

    if (available_count == 0 && add_count == 0
        && alive_count + 1 < config->max_children
        && alive_count + 1 < MAX_CHILD_COUNT){
            add_count = 1;
    }

    for (i = 0; i < used_children && add_count > 0; i++){
        if (children[i].state == SERVER_ITEM_DEAD){
            fork_child(children + i);
            add_count--;
            available_count++;
        }
    }

    if (add_count > 0){
        for(i = used_children; i < (used_children + add_count); i++){
            fork_child(children + i);
        }
        used_children += add_count;
    }
}

void stop_server(){
    if (server_socket > 0){
        close(server_socket);
        server_socket = 0;
    }

    int i = 0;
    for(; i < used_children; i++){
        if (children[i].state != SERVER_ITEM_DEAD){
             kill(children[i].pid, SIGKILL);
        }
    }
}

void sigchld_handler(int sig)
{
    pid_t p;
    int status;

    while ((p=waitpid(-1, &status, WNOHANG)) != -1)
    {
        // handling death of a child
        int i;
        for(i=0;i<used_children;i++){
            if (children[i].pid == p){
                children[i].state = SERVER_ITEM_DEAD;
                break;
            }
        }
    }
}


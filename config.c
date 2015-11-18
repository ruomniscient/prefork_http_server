#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "config_parser.h"
#include "utils.h"


struct Config *currentConfig =  NULL;

char *read_host(struct Config *config, char *buf);
char *read_block(struct Config *config, char *buf);

struct Config *config_get(){
    if(!currentConfig){
        currentConfig = (struct Config *)malloc(sizeof(struct Config));

        currentConfig->bind_to = "127.0.0.1:80";
        currentConfig->child_max_queries = 100;
        currentConfig->min_children = 1;
        currentConfig->max_children = 2;
        currentConfig->hosts_count = 0;
    }
    return currentConfig;
}

void config_read_from_file(FILE *file){
    int length = 0;
    char *buffer;

    fseek (file, 0, SEEK_END);
    length = ftell (file);
    fseek (file, 0, SEEK_SET);
    buffer = malloc (length+1);
    memset(buffer, 0, length+1);
    fread (buffer, 1, length, file);

    Config *newConf = (Config *)malloc(sizeof(Config));
    newConf->hosts_count = 0;
    while(*buffer){
        buffer = ltrim(buffer);
        buffer = read_block(newConf, buffer);
        buffer = ltrim(buffer);
    }
    free(buffer);
    currentConfig = newConf;
}

char *read_block(Config *config, char *buf){
    buf = ltrim(buf);
    char ident[64], str[STRING_BUFFER];
    buf = read_ident(buf, ident, 64 - 1);

    buf = ltrim(buf);
    if(strcmp(ident, "min_children") == 0){
        buf = read_int(buf, &(config->min_children));
    }else if(strcmp(ident, "max_children") == 0){
        buf = read_int(buf, &(config->max_children));
    }else if(strcmp(ident, "child_max_queries") == 0){
        buf = read_int(buf, &(config->child_max_queries));
    }else if(strcmp(ident, "bind") == 0){
        buf = read_string(buf, &str, STRING_BUFFER - 1);
        config->bind_to = strdup(str);
    }else if(strcmp(ident, "host") == 0){
        buf = ltrim(consume(ltrim(buf), "{"));
        buf = read_host(config, buf);
        buf = consume(ltrim(buf), "}");
    }else{
        die_with_error("unknown identifier");
    }

    buf = consume(ltrim(buf), ";");
    return buf;
}

char *read_host(Config *config, char *buf){
    int hostId = config->hosts_count;
    if (hostId >= HOSTS_LIMIT){
        die_with_error("hosts limit reached");
    }
    config->hosts_count++;

    char ident[64], str[STRING_BUFFER];
    while(1){
        buf = ltrim(buf);
        buf = read_ident(buf, ident, 64 - 1);

        buf = ltrim(buf);
        if(strcmp(ident, "mask") == 0){
            do{
                buf = read_string(buf, &str, STRING_BUFFER - 1);

                MaskList *newMask = (MaskList *)malloc(sizeof(MaskList));
                newMask->mask = strdup(buf);
                newMask->next = config->hosts[hostId].mask;
                config->hosts[hostId].mask = newMask;

                buf = ltrim(buf);
            }while(*buf != ';');
        }else if(strcmp(ident, "root") == 0){
            buf = read_string(buf, &str, STRING_BUFFER - 1);
            config->hosts[hostId].root = strdup(str);
        }else if(strlen(ident) == 0){
            break;
        }
        else{
            die_with_error("unknown identifier2 ");
        }

        buf = consume(ltrim(buf), ";");
    }
    return buf;
}



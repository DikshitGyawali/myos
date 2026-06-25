#include <drivers/keyboard.h>
#include <IO/screen.h>
#include <shell/shell.h>
#include <stddef.h>

#define MAX_COMMAND_SIZE 256

ShellCommand g_CMDHandler[MAX_COMMAND_SIZE];


void shell_CMD_Handler(char* command_name, char* args);
void basic_commands();
int strcmp(const char* a, const char* b){
    while (*a && *b){
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return *a - *b;
}


void shell_ReadLine(char* buf, int max){
    int i = 0;
    while(1){
        char c = kb_getchar();

        if (c == '\n'){
            buf[i] = 0;
            putchar('\n');
            return;
        }
        if (c == '\b'){
            if (i > 0){
                i--;
                putchar('\b');
            }
            continue;
        }
        if(i < max - 1){
            buf[i++] = c;
            putchar(c);
        }
    }
}


void shell_execute(char* cmd)
{
    if(cmd[0] == '\0') return;

    char cmd_name[32];
    for(int i = 0;i < 32; ++i){
        if (cmd[i] == '\0'){
            cmd_name[i] = '\0';
            shell_CMD_Handler(cmd_name, 0);
            return;
        }
        if (cmd[i] == ' '){
            cmd_name[i] = '\0';
            shell_CMD_Handler(cmd_name, &cmd[i+1]);
            return;
        }
        cmd_name[i] = cmd[i];
    }
    kprintf("Command too long\n");
}


void shell_Init() // called by kernel
{
    basic_commands();
    char buffer[128];

    while (1)
    {
        putchar_withcolor('>', 0x03);
        putchar(' ');

        shell_ReadLine(buffer, 128);
        shell_execute(buffer);
    }
}


void shell_RegisterCommand(ShellCommand cmd){
    for (int i = 0; i < MAX_COMMAND_SIZE; ++i){
        if(g_CMDHandler[i].name == NULL){
            g_CMDHandler[i] = cmd;
            return;
        }
    }
}

void shell_CMD_Handler(char* command_name, char* args){
    for(int i = 0; i < MAX_COMMAND_SIZE; ++i){
        if(g_CMDHandler[i].name == NULL) return;
        if(!strcmp(g_CMDHandler[i].name, command_name)){
            g_CMDHandler[i].handler(args);
            return;
        }
    }
    kprintf("Unknown Command\n");
}


void help(char* args){
    (void)args;
    kprintf("List of Commands are:\n");
    for (int i = 0; i < MAX_COMMAND_SIZE; ++i){
        if(g_CMDHandler[i].name == NULL){
            return;
        }
        kprintf("%s\n", g_CMDHandler[i].name);
    }
}

void clear(char* args){
    (void)args;
    clear_screen();
}

void crash(char* args){
    (void)args;
    [[maybe_unused]] int a = 1/0;
}

void echo(char* args){
    if (args == NULL || args[0] == '\0') return;
    kprintf("%s\n", args);
}

void basic_commands(){
    shell_RegisterCommand((ShellCommand){"help", help});
    shell_RegisterCommand((ShellCommand){"echo", echo});
    shell_RegisterCommand((ShellCommand){"clear", clear});
    shell_RegisterCommand((ShellCommand){"crash", crash});
}

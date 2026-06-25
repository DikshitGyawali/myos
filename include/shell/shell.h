#ifndef KERNEL_SHELL_H
#define KERNEL_SHELL_H

typedef void (*CommandHandler)(char* args);

typedef struct {
    const char* name;
    CommandHandler handler;
} ShellCommand;

void shell_Init();
void shell_RegisterCommand(ShellCommand cmd);


#endif

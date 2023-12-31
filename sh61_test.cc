#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#undef exit
#define exit __DO_NOT_CALL_EXIT__READ_PROBLEM_SET_DESCRIPTION__


// struct command, pipeline, conditional
//    Data structures forming a basis for tree representation.

struct command {
    std::vector<std::string> args;
    pid_t pid = -1;      // process ID running this command, -1 if none
    command();
    ~command();

    command* pipe_next = nullptr;
    command* pipe_prev = nullptr;

    void run();
};

struct pipeline {
    command* cmd_ch = nullptr;
    pipeline* cond_next = nullptr;
    bool next_or = false;
};

struct conditional {
    pipeline* pipe_ch = nullptr;
    conditional* list_next = nullptr;
    bool is_bg = false;
};


conditional* parse_conditional(shell_parser const parser, shell_token_iterator it);
pipeline* parse_pipeline(shell_parser const& parser, shell_token_iterator& it);
command* parse_command(shell_parser const parser, shell_token_iterator it);
std::vector<conditional*> cds;
std::vector<pipeline*> pls;
std::vector<command*> cmds;

command::command() {
}

command::~command() {
}

// COMMAND EXECUTION

// command::run()
//    Creates a single child process running the command in `this`, and
//    sets `this->pid` to the pid of the child process.
//
//    If a child process cannot be created, this function should call
//    `_exit(EXIT_FAILURE)` (that is, `_exit(1)`) to exit the containing
//    shell or subshell. If this function returns to its caller,
//    `this->pid > 0` must always hold.
//
//    Note that this function must return to its caller *only* in the parent
//    process. The code that runs in the child process must `execvp` and/or
//    `_exit`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//       This will require creating a vector of `char*` arguments using
//       `this->args[N].c_str()`. Note that the last element of the vector
//       must be a `nullptr`.
//    PART 4: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.

void command::run() {
    assert(this->pid == -1);
    assert(this->args.size() > 0);

    // create char* args list
    char* chargs[args.size() + 1];
    for (size_t i = 0; i != this->args.size(); ++i) {
        chargs[i] = (char*)this->args[i].c_str();
    }
    chargs[args.size()] = nullptr;
    
    // spawn child proc
    pid_t chid = fork();
    if (chid == 0) {
        int s = execvp(chargs[0], chargs);
        if (s == -1) {
            _exit(EXIT_FAILURE);
        }
    }
    waitpid(chid, NULL, 0);
}


// run_list(c)
//    Run the command *list* starting at `c`. Initially this just calls
//    `c->run()` and `waitpid`; youâ€™ll extend it to handle command lists,
//    conditionals, and pipelines.
//
//    It is possible, and not too ugly, to handle lists, conditionals,
//    *and* pipelines entirely within `run_list`, but many students choose
//    to introduce `run_conditional` and `run_pipeline` functions that
//    are called by `run_list`. Itâ€™s up to you.
//
//    PART 1: Start the single command `c` with `c->run()`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in `command::run` (or in helper functions).
//    PART 2: Introduce a loop to run a list of commands, waiting for each
//       to finish before going on to the next.
//    PART 3: Change the loop to handle conditional chains.
//    PART 4: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 5: Change the loop to handle background conditional chains.
//       This may require adding another call to `fork()`!

void run_conditional(conditional* cd, command* c) {
}

void run_pipeline(pipeline* pl) {
}

void run_list(command* c) {
    //c->run();
    for (int i = 0; i != cmds.size(); ++i) {
        cmds[i]->run();
        waitpid(cmds[i]->pid, NULL, 0);
    }
    cmds.clear();
}


// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if
//    `s` is empty (only spaces). Youâ€™ll extend it to handle more token
//    types.
command* parse_line(const char* s) {
    shell_parser parser(s);
    // Your code here!

    command* c = nullptr;
    for (shell_token_iterator it = parser.begin(); it != parser.end(); ++it) {
        if (!c && it.type() == TYPE_NORMAL) {
             c = new command;
             cmds.push_back(c);
         }
         c->args.push_back(it.str());
        //if (it.type() == TYPE_SEQUENCE) {
            //parse_conditional(parser, it);
        //}
    }
    return (command*)cmds[0];
}

conditional* parse_conditional(shell_parser const parser, shell_token_iterator it) {
    conditional* cd = nullptr;
    while (it != parser.end()) {
        switch (it.type()) {
            case TYPE_SEQUENCE :
                cd = new conditional;
                cd->is_bg = false;
                ++it;
                break;
            case TYPE_BACKGROUND :
                cd = new conditional;
                cd->is_bg = true;
                ++it;
                break;
            default :
                break;
        }
        cds.push_back(cd);
        parse_command(parser, it);
        ++it;
    }
    return cd;
}

command* parse_command(shell_parser const parser, shell_token_iterator it) {
    command* c = nullptr;
    // get commands from pipelines/conditionals
    while (it != parser.end()) {
        switch (it.type()) {
            c = new command;
            case TYPE_SEQUENCE :
            case TYPE_BACKGROUND :
            case TYPE_PIPE :
            case TYPE_AND :
            case TYPE_OR :
                break;
            default :
                break;
        }
        if (!c) {
            cmds.push_back(c);
        }
        cmds[cmds.size()-1]->args.push_back(it.str());
        ++it;
    }
    return c;
    
}

// all: returns your entire tree, so everything up to end
// conditional: returns a conditional, AKA everything up to a ; or & or end of line, whichever comes first
// pipeline: return a pipeline AKA everything up to a &&, &, ;, ||, or end of line, whichever comes first
// command: return a command AKA everything up to a &&, &, ; ||, &&, or |, whichever comes first 

pipeline* parse_pipeline(shell_parser const& parser, shell_token_iterator& it) {
    pipeline* pl = nullptr;

    // get pipelines from conditionals
    while (it != parser.end()) {
        switch (it.type()) {
            case TYPE_SEQUENCE :
                break;
            case TYPE_BACKGROUND : 
                break;
            case TYPE_AND :
                break; // change later
        }
    }
    return pl;
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for `-q` option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            return 1;
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            if (command* c = parse_line(buf)) {
                run_list(c);
                delete c;
            }
            bufpos = 0;
            needprompt = 1;
        }
    }

    return 0;
}
#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>

#undef exit
#define exit __DO_NOT_CALL_EXIT__READ_PROBLEM_SET_DESCRIPTION__

volatile sig_atomic_t flag = 0;

struct list {
    std::vector<conditional> conds_child;
};

struct command {
    std::vector<std::string> args;
    pid_t pid = -1;
    command();
    ~command();
    int run();

    bool right_of_pipe = true;
    bool left_of_pipe = false;
    std::vector<std::string> redir = {">", "<", "2>", "1>", ">>", "2>>"};
    int pgid = -1;
    bool is_cd = false;
    int midfd = -1;
    int estatus = 0;
};

struct conditional {
    std::vector<pipeline> pipe_child;
    bool background = false;
};

struct pipeline {
    std::vector<command> cmd_child;
    bool next_is_or = false;
};


// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command() {
}

// command::~command()
//    This destructor function is called to delete a command.

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


int command::run() {
    assert(this->pid == -1);
    assert(this->args.size() > 0);

    // create char* args for execvp
    char* chargs[this->args.size()+1];
    unsigned int i = 0;
    while (i != this->args.size()) {
        chargs[i] = (char*)this->args[i].c_str();
        ++i;
    }
    chargs[this->args.size()] = nullptr;

    // pipe dance
    int pipefd[2];
    if (this->left_of_pipe) {
        int s = pipe(pipefd);
        assert(s == 0);
    }
    pid_t p = fork();
    if (p == 0) {
        this->pid = p;
        // first process generates pgid
        if (!this->right_of_pipe) {
            setpgid(this->pid, 0);
            this->pgid = this->pid;
        }
        else {
            setpgid(this->pid, this->pgid);
        }

        if (this->midfd != 0) {
            dup2(this->midfd, STDIN_FILENO);
            close(this->midfd);
        }
        if (this->left_of_pipe) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
        }


        // handle redirections, error messages
        int fd = 0;
        if (this->redir[0] != ">") {
            fd = open(this->redir[0].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                fprintf(stderr, "No such file or directory \n");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (this->redir[1] != "<") {
            fd = open(this->redir[1].c_str(), O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "No such file or directory \n");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (this->redir[2] != "2>") {
            fd = open(this->redir[2].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                fprintf(stderr, "No such file or directory \n");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        if (this->redir[3] != "1>") {
            fd = open(this->redir[3].c_str(), O_WRONLY | O_CREAT, 0644);
            if (fd < 0) {
                fprintf(stderr, "No such file or directory \n");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (this->redir[4] != ">>") {
            fd = open(this->redir[4].c_str(), O_WRONLY | O_APPEND, 0644);
            if (fd < 0) {
                fprintf(stderr, "No such file or directory \n");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (this->redir[5] != "2>>") {
            fd = open(this->redir[5].c_str(), O_WRONLY | O_APPEND, 0644);
            if (fd < 0) {
                fprintf(stderr, "No such file or directory \n");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        // handle cd
        if (this->is_cd) {
            int s = chdir(this->args[1].c_str());
            if (s == 0) {
                _exit(EXIT_SUCCESS);
            }
            else {
                _exit(EXIT_FAILURE);
            }
        }
        // execvp, but only not if cd
        execvp(chargs[0], chargs);
        _exit(EXIT_FAILURE);
    }

    // parent process
    this->pid = p;
    
    // first process generates pgid
    if (!this->right_of_pipe) {
        setpgid(0, 0);
        this->pgid = 0;
    }
    else {
        setpgid(this->pid, this->pgid);
    }

    if (this->right_of_pipe) {
        close(this->midfd);
    }
    if (this->left_of_pipe) {
        close(pipefd[1]);
        this->midfd = pipefd[0];
    }

    // handle cd
    if (this->is_cd) {
        int s = chdir(chargs[1]);
        assert(s <= 0);
    }

    // return previous read end
    return this->midfd;
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

void run_list(list ls) {
    unsigned int i = 0;
    while (i != ls.conds_child.size()) {
        if (ls.conds_child[i].background == true) {
            pid_t ch = fork();
            if (ch == 0) {
                run_conditional(ls.conds_child[i], 0);
                _exit(EXIT_SUCCESS);
            }
        }
        else {
            run_conditional(ls.conds_child[i], 1);
        }
        ++i;
    }
}

void run_conditional(conditional cd, int fg) {
    
    unsigned int i = 0;
    while (i < cd.pipe_child.size()) {
        // run first
        int wstatus = 0;
        pid_t p = run_pipeline(cd.pipe_child[i]);

        // check interrupts don't stop bg        
        if (fg) {
            claim_foreground(p);
            waitpid(p, &wstatus, 0);
            claim_foreground(0);
        }
        else {
            waitpid(p, &wstatus, 0);
        }

        int s = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;

        if ((cd.pipe_child[i].next_is_or == false && s == 0)
         || (cd.pipe_child[i].next_is_or == true  && s != 0)) {
            // run i+1th pipeline on next iteration
            ++i;
        }
        else {
            unsigned int j = i;
            // loop until different operator
            while (j < cd.pipe_child.size()
            && cd.pipe_child[j].next_is_or == cd.pipe_child[i].next_is_or) {
                ++j;
            }
            i = j+1;
        }
    }
}


pid_t run_pipeline(pipeline pl) {
    pid_t p = 0;

    unsigned int i = 0;
    pl.cmd_child[0].right_of_pipe = false;
    while (i != pl.cmd_child.size()) {
        // if not first command, set pgid to previous pgid
        if (i != 0) {
            pl.cmd_child[i].pgid = pl.cmd_child[i-1].pgid;
        }
        
        // run command
        int curr_fd = pl.cmd_child[i].run();

        // if not last command, pass previous read end
        if (i != pl.cmd_child.size()-1) {
            pl.cmd_child[i+1].midfd = curr_fd;
        }
        p = pl.cmd_child[i].pid;
        ++i;
    }
    return p;
}    


// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if
//    `s` is empty (only spaces). Youâ€™ll extend it to handle more token
//    types.

list parse_line(const char* s) {
    shell_parser parser(s);
    list ls;
    shell_token_iterator it = parser.begin();
    while (it != parser.end()) {
        switch(it.type()) {
            case TYPE_SEQUENCE :
            case TYPE_BACKGROUND :
                break;
            default :
                conditional r = parse_conditional(parser, it);
                ls.conds_child.push_back(r);
        }
        ++it;
    }
    return ls;
}


conditional parse_conditional(shell_parser& parser, shell_token_iterator& it) {
    conditional cd;
    while (it != parser.end()) {
        switch (it.type()) {
            case TYPE_SEQUENCE :
                // set foreground, background
                cd.background = false;
                return cd;
            case TYPE_BACKGROUND :
                cd.background = true;
                return cd;
            case TYPE_AND :
            case TYPE_OR :
                ++it;
                break;
            default :
                pipeline r = parse_pipeline(parser, it);
                cd.pipe_child.push_back(r);
        }
    }
    return cd;
}


pipeline parse_pipeline(shell_parser& parser, shell_token_iterator& it) {
    pipeline pl;
    while (it != parser.end()) {
        switch (it.type()) {
            case TYPE_SEQUENCE :
            case TYPE_BACKGROUND :
                return pl;
            case TYPE_AND :
                // set next as and, or
                pl.next_is_or = false;
                return pl;
            case TYPE_OR :
                pl.next_is_or = true;
                return pl;
            case TYPE_PIPE :
                ++it;
                break;
            default :
                command r = parse_command(parser, it);
                pl.cmd_child.push_back(r);
        }
    }
    return pl;
}


command parse_command(shell_parser& parser, shell_token_iterator& it) {
    command c;
    while (it != parser.end()) {
        switch(it.type()) {
            case TYPE_SEQUENCE :
            case TYPE_BACKGROUND :
            case TYPE_AND :
            case TYPE_OR :
                return c;
            case TYPE_PIPE :
                c.left_of_pipe = true;
                return c;
            case TYPE_REDIRECT_OP :
                if (it.str() == ">") {
                    // load filename
                    ++it;
                    c.redir[0] = it.str();
                }
                else if (it.str() == "<") {
                    ++it;
                    c.redir[1] = it.str();
                }
                else if (it.str() == "2>") {
                    ++it;
                    c.redir[2] = it.str();
                }
                else if (it.str() == "1>") {
                    ++it;
                    c.redir[3] = it.str();
                }
                else if (it.str() == ">>") {
                    ++it;
                    c.redir[4] = it.str();
                }
                else if (it.str() == "2>>") {
                    ++it;
                    c.redir[5] = it.str();
                }
                break;              
            default :
                if (it.str() == "cd") {
                    c.is_cd = true;
                }
                c.args.push_back(it.str());
        }
        ++it;
    }
    return c;
}

void signal_handler(int signal) {
    flag = signal;
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
    set_signal_handler(SIGINT, signal_handler); 

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
            list ls = parse_line(buf);
            if (!ls.conds_child.empty()) {
                run_list(ls);
            }
            bufpos = 0;
            needprompt = 1;
        }
        // reap zombies
        int status = 0;
        while (waitpid(-1, &status, WNOHANG) > 0);

        // handle interrupts
        if (flag != 0) {
            kill(getpid(), SIGINT);
            needprompt = true;
            flag = 0;
            continue;
        }
    }

    return 0;
}


// print tree functions for debugging

void print_command(command c){
    printf("command: \n");
    for(unsigned int i = 0; i < c.args.size(); i++){
        printf("%s ", c.args[i].c_str());
    }
    puts("");
}

void print_pipeline(pipeline pl){
    printf("pipeline: \n");
    for(unsigned int i = 0; i < pl.cmd_child.size(); i++){
        print_command(pl.cmd_child[i]);
        printf("left_of_pipe: %d\n", pl.cmd_child[i].left_of_pipe);
    }
}

void print_conditional(conditional cd){
    printf("conditional: \n");
    for(unsigned int i = 0; i < cd.pipe_child.size(); i++){
        print_pipeline(cd.pipe_child[i]);
        printf("is_or: %d\n", cd.pipe_child[i].next_is_or);
    }
}

void print_list(list ls){
    printf("list:\n");
    for(unsigned int i = 0; i < ls.conds_child.size(); i++){
        print_conditional(ls.conds_child[i]);
    }
}

void print_args (command c) {
    unsigned int i = 0;
    while (i < c.args.size()) {
        std::cout << c.args[i].c_str() << "\n";
        ++i;
    }
}
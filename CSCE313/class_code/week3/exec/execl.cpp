/*
Example: execl
 */

#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace std;

int main()
{
    /*
    const char *file = "/usr/bin/echo";
    const char *arg1 = "Hello world!";
    execl(file, file, arg1, NULL); // Binary + Path to the Binary + Rest of the options ... + NULL
    */
    //const char *file = "/bin/sh"; //shell
    const char *file = "/home/ilans/csce313/math-scripts/factorial.sh"; //script
    execl(file, file, NULL);
    return 0;
}

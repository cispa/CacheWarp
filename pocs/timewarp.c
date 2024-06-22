#include <stdio.h>
#include <string.h>
int return0(){
    return 0;
}
int main(int argc, char **argv){
    while(1){
        // If we take static input, strcmp is optimized out
        if(strcmp(argv[0], "password") == 0){
            puts("Win!");
        }
        return0();
    }
}
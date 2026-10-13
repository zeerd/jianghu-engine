chcp 65001 | Out-Null; [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; $OutputEncoding = [System.Text.Encoding]::UTF8
cppcheck --enable=warning --suppress=*:3rd\sqlite\*  src > bin\cppcheck.result.txt 2>&1

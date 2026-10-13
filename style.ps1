.\Remove-LuaBom.ps1 -Path "c:\jianghu-engine\lua-default\"
.\Remove-LuaBom.ps1 -Path "c:\jianghu-engine\lua-sample\"
.\Remove-LuaBom.ps1 -Path "c:\jianghu-engine\tests\lua-tests\"

Get-ChildItem -Path src -Recurse -Include *.cpp | ForEach-Object { clang-format -i $_.FullName }
Get-ChildItem -Path src -Recurse -Include *.inl | ForEach-Object { clang-format -i $_.FullName }
Get-ChildItem -Path src -Recurse -Include *.h | ForEach-Object { clang-format -i $_.FullName }
Get-ChildItem -Path tests -Recurse -Include *.cpp | ForEach-Object { clang-format -i $_.FullName }
Get-ChildItem -Path tests -Recurse -Include *.h | ForEach-Object { clang-format -i $_.FullName }

Get-ChildItem -Path lua-default -Recurse -Include *.lua | ForEach-Object { stylua $_.FullName }
Get-ChildItem -Path lua-sample -Recurse -Include *.lua | ForEach-Object { stylua $_.FullName }

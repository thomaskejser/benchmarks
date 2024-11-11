
param (
    [string]$arg
)

$out_file = "benchmark.csv"
$cpp = @{ Name="C++"; Path = "./cpp/bin/Release/crud.exe"; Args = @() }
$python= @{ Name="Python"; Path = "python"; Args=@("python/crud.py") }
$cs = @{ Name="C#"; Path = "./cs/bin/Release/crud.exe"; Args = @() }


$binaries = @($cpp, $python, $cs)
$tests = ("simple", "json", "process", "thread", "async") 

Remove-Item $out_file -ErrorAction SilentlyContinue 

Add-Content -Path $out_file -Value "language|test|ops_per_sec|last_value"

foreach ($binary in $binaries) {
  Write-Output  $binary.Name
  foreach($test in $tests) {
    Write-Output "...$test $arg"
    & $binary.Path $binary.Args $arg $test 
  }
}

Write-Output "Results"
Get-Content $out_file

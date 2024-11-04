
param (
    [string]$arg
)

$out_file = "benchmark.csv"
$cpp = @{ Path = "./cpp/bin/Release/crud.exe"; Args = @() }
$python= @{ Path = "python"; Args=@("python/crud.py") }
$cs = @{ Path = "./cs/bin/Release/crud.exe"; Args = @() }


$binaries = @($cpp, $python, $cs)
$tests = ("simple", "json", "process", "thread", "async") 

Remove-Item $out_file -ErrorAction SilentlyContinue 

Add-Content -Path $out_file -Value "language|test|ops_per_sec|last_value"

foreach ($binary in $binaries) {
  foreach($test in $tests) {
    & $binary.Path $binary.Args $arg $test 
  }
}

echo "Results"
cat $out_file

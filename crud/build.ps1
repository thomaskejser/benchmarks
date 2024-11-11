$originalLocation = Get-Location
# build C++
try {
    Set-Location cpp
    New-Item -ItemType Directory -Path ./build -Force
    Set-Location build
    cmake ..
    Set-Location ..
    cmake --build .\build\ --config Release -- /p:Optimize=true
}
finally {
    Set-Location $originalLocation
}

# build C#
try {
    Set-Location $originalLocation/cs
    dotnet build -c Release
}
finally {
    Set-Location $originalLocation
}

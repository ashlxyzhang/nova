# PowerShell script to compile shaders into header files
# Recursively finds all .vert, .frag, and .comp files and compiles them

param(
    [string]$ShaderDir = $PSScriptRoot
)

Write-Host "Compiling shaders in: $ShaderDir" -ForegroundColor Green

# Get all shader files
$shaderFiles = Get-ChildItem -Path $ShaderDir -Include *.vert, *.frag, *.comp -Recurse

if ($shaderFiles.Count -eq 0) {
    Write-Host "No shader files found!" -ForegroundColor Yellow
    exit 0
}

Write-Host "Found $($shaderFiles.Count) shader files to compile..." -ForegroundColor Cyan

foreach ($shaderFile in $shaderFiles) {
    # Generate header filename with extension (e.g., spinning_cube.vert -> spinning_cube_vert.h)
    $extension = $shaderFile.Extension.TrimStart('.')
    $headerName = $shaderFile.BaseName + "_" + $extension + ".h"
    $headerPath = Join-Path $shaderFile.DirectoryName $headerName
    
    # Generate variable name to match header filename (e.g., spinning_cube.vert -> spinning_cube_vert)
    $variableName = $shaderFile.BaseName + "_" + $extension
    
    Write-Host "`nCompiling: $($shaderFile.Name) -> $headerName" -ForegroundColor Yellow
    
    # Run glslang (note: uses glslang, not glslangValidator)
    try {
        $result = glslang `
            --target-env vulkan1.0 `
            --vn $variableName `
            -o $headerPath `
            $shaderFile.FullName
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✓ Successfully compiled $($shaderFile.Name)" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Failed to compile $($shaderFile.Name)" -ForegroundColor Red
            Write-Host "  glslang exited with code: $LASTEXITCODE" -ForegroundColor Red
        }
    } catch {
        Write-Host "  ✗ Error: $_" -ForegroundColor Red
    }
}

Write-Host "`nShader compilation complete!" -ForegroundColor Green

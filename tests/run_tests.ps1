$ErrorActionPreference = "Stop"
$compiler = Get-Command gcc -ErrorAction Stop
$parserSource = Join-Path (Split-Path -Parent $PSScriptRoot) "parser.c"
$testSource = Join-Path $PSScriptRoot "test_parser.c"

foreach ($standard in @("c11", "c99")) {
    $output = Join-Path ([System.IO.Path]::GetTempPath()) "mysh-parser-$PID-$standard.exe"
    try {
        & $compiler.Source "-std=$standard" "-Wall" "-Wextra" "-Wpedantic" `
            "-Werror" $parserSource $testSource "-o" $output
        if ($LASTEXITCODE -ne 0) {
            throw "Compilation failed for $standard (exit $LASTEXITCODE)."
        }

        & $output
        if ($LASTEXITCODE -ne 0) {
            throw "Parser tests failed for $standard (exit $LASTEXITCODE)."
        }
    }
    finally {
        Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
    }
}

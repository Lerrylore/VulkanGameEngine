param(
    [string]$InputDirectory = (Join-Path $PSScriptRoot "..\RenderGraphDumps"),
    [string]$GraphvizPath = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GraphvizPath))
{
    $dotCommand = Get-Command dot.exe -ErrorAction SilentlyContinue
    if ($null -ne $dotCommand)
    {
        $GraphvizPath = $dotCommand.Source
    }
    else
    {
        $programFiles = [Environment]::GetFolderPath("ProgramFiles")
        $programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
        $candidates = @(
            (Join-Path $programFiles "Graphviz\bin\dot.exe"),
            (Join-Path $programFilesX86 "Graphviz\bin\dot.exe")
        )

        $GraphvizPath = $candidates |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1
    }
}

if ([string]::IsNullOrWhiteSpace($GraphvizPath) -or -not (Test-Path -LiteralPath $GraphvizPath))
{
    throw "Graphviz dot.exe non trovato. Installa Graphviz oppure passa -GraphvizPath <percorso>"
}

$resolvedInputDirectory = Resolve-Path -LiteralPath $InputDirectory
$dotFiles = Get-ChildItem -LiteralPath $resolvedInputDirectory -Filter "*.dot" -File

if ($dotFiles.Count -eq 0)
{
    throw "Nessun dump .dot trovato in '$resolvedInputDirectory'. Avvia il renderer almeno una volta."
}

foreach ($dotFile in $dotFiles)
{
    $svgPath = Join-Path $dotFile.DirectoryName ($dotFile.BaseName + ".svg")
    $pngPath = Join-Path $dotFile.DirectoryName ($dotFile.BaseName + ".png")

    & $GraphvizPath -Tsvg $dotFile.FullName -o $svgPath
    if ($LASTEXITCODE -ne 0)
    {
        throw "Graphviz non ha generato '$svgPath'"
    }

    & $GraphvizPath -Tpng -Gdpi=160 $dotFile.FullName -o $pngPath
    if ($LASTEXITCODE -ne 0)
    {
        throw "Graphviz non ha generato '$pngPath'"
    }

    Write-Host "Generati: $svgPath e $pngPath"
}

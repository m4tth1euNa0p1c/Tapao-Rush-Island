<#
.SYNOPSIS
  Toolbox CLI pour monke_island (UE 5.8, C++).

.EXAMPLE
  ./scripts/ue.ps1 build            # compile monke_islandEditor Win64 Development
  ./scripts/ue.ps1 build -Config DebugGame
  ./scripts/ue.ps1 gen              # regenere les fichiers projet (.sln)
  ./scripts/ue.ps1 test             # lance les tests d'automatisation (headless)
  ./scripts/ue.ps1 test -Filter monke_island.Combat
  ./scripts/ue.ps1 editor           # ouvre l'editeur
  ./scripts/ue.ps1 status           # editeur ouvert ? serveur MCP joignable ?
  ./scripts/ue.ps1 clean            # supprime Binaries/Intermediate du projet
  ./scripts/ue.ps1 profile -Map /Game/FirstPerson/Lvl_FirstPerson -Frames 1800   # capture CSV perf (jeu standalone)
  ./scripts/ue.ps1 package -Config Shipping                                      # BuildCookRun -> build/package
#>
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'rebuild', 'gen', 'test', 'editor', 'status', 'clean', 'profile', 'package')]
    [string]$Command = 'build',

    [ValidateSet('Development', 'DebugGame', 'Shipping')]
    [string]$Config = 'Development',

    [string]$Filter = 'monke_island',

    [string]$Map = '/Game/FirstPerson/Lvl_FirstPerson',

    [int]$Frames = 1800,

    [string]$Version = '0.1.0',

    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$Root        = Split-Path -Parent $PSScriptRoot
$ProjectDir  = Join-Path $Root 'monke_island'
$Uproject    = Join-Path $ProjectDir 'monke_island.uproject'
$EngineDir   = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$BuildBat    = Join-Path $EngineDir 'Build\BatchFiles\Build.bat'
$EditorExe   = Join-Path $EngineDir 'Binaries\Win64\UnrealEditor.exe'
$EditorCmd   = Join-Path $EngineDir 'Binaries\Win64\UnrealEditor-Cmd.exe'
$UBT         = Join-Path $EngineDir 'Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe'
$RunUAT      = Join-Path $EngineDir 'Build\BatchFiles\RunUAT.bat'
$McpUrl      = 'http://127.0.0.1:8000/mcp'
$Target      = if ($Config -eq 'Shipping') { 'monke_island' } else { 'monke_islandEditor' }

function Test-EditorRunning {
    [bool](Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue)
}

function Test-Mcp {
    try {
        $body = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"ue.ps1","version":"1"}}}'
        $null = Invoke-WebRequest -Uri $McpUrl -Method Post -Body $body -ContentType 'application/json' `
            -Headers @{ Accept = 'application/json, text/event-stream' } -TimeoutSec 3 -UseBasicParsing
        return $true
    } catch { return $false }
}

function Invoke-Build([switch]$Clean) {
    if ((Test-EditorRunning) -and $Target -eq 'monke_islandEditor' -and -not $Force) {
        Write-Host "[ue] L'editeur est ouvert : UBT refusera de compiler si Live Coding est actif." -ForegroundColor Yellow
        Write-Host "[ue] -> Utilise Live Coding via MCP (LiveCodingToolset.CompileLiveCoding) ou Ctrl+Alt+F11 dans l'editeur." -ForegroundColor Yellow
        Write-Host "[ue] -> Ou ferme l'editeur, ou relance avec -Force." -ForegroundColor Yellow
        exit 2
    }
    $ubtArgs = @($Target, 'Win64', $Config, "-Project=$Uproject", '-WaitMutex', '-NoHotReloadFromIDE')
    if ($Clean) {
        & $BuildBat @ubtArgs '-Clean'
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    & $BuildBat @ubtArgs
    exit $LASTEXITCODE
}

# Verrou de l'editeur partage (scripts/editor_lock.py) : une seule session a la fois compile l'arbre principal,
# lance la suite headless, (re)lance l'editeur, nettoie ou package. Proprietaire : $env:MONKE_LOCK_OWNER.
$LockScript = Join-Path $PSScriptRoot 'editor_lock.py'
$LockOwner  = if ($env:MONKE_LOCK_OWNER) { $env:MONKE_LOCK_OWNER } else { "ue.ps1@$env:USERNAME-pid$PID" }
$TookLock   = $false
if ($Command -in 'build', 'rebuild', 'test', 'editor', 'clean', 'package') {
    python $LockScript check --owner $LockOwner
    if ($LASTEXITCODE -ne 0 -and -not $Force) {
        Write-Host "[ue] Editeur verrouille par une autre session (voir ci-dessus) : attends, ou demande-lui (python scripts/editor_lock.py status)." -ForegroundColor Red
        exit 3
    }
    if ($Command -ne 'editor') {
        # already held (by this owner, checked above)? keep it; free? take it for the command
        python $LockScript check --owner '__nobody__' 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) {
            python $LockScript acquire --owner $LockOwner --reason "ue.ps1 $Command" --minutes 45 | Out-Null
            $TookLock = $true
        }
    }
}

try {
switch ($Command) {
    'build'   { Invoke-Build }
    'rebuild' { Invoke-Build -Clean }

    'gen' {
        & $UBT -projectfiles "-project=$Uproject" -game -rocket -progress
        exit $LASTEXITCODE
    }

    'test' {
        if (Test-EditorRunning) {
            Write-Host "[ue] Editeur ouvert : prefere lancer les tests via MCP (AutomationTestToolset) pour ne pas bloquer." -ForegroundColor Yellow
        }
        $report = Join-Path $ProjectDir 'Saved\Automation\Reports'
        & $EditorCmd $Uproject "-ExecCmds=Automation RunTests $Filter; Quit" `
            -unattended -nullrhi -nosplash -nosound -NoP4 -stdout -FullStdOutLogOutput `
            "-ReportExportPath=$report"
        $code = $LASTEXITCODE
        Write-Host "[ue] Rapport : $report\index.json"
        exit $code
    }

    'editor' {
        Start-Process $EditorExe -ArgumentList "`"$Uproject`""
        Write-Host "[ue] Editeur lance. Le serveur MCP demarre automatiquement sur $McpUrl"
    }

    'status' {
        $ed  = Test-EditorRunning
        $mcp = Test-Mcp
        Write-Host ("Editeur ouvert : {0}" -f ($(if ($ed) { 'oui' } else { 'non' })))
        Write-Host ("MCP ($McpUrl) : {0}" -f ($(if ($mcp) { 'OK' } else { 'injoignable' })))
        if ($ed -and -not $mcp) {
            Write-Host "-> Dans la console de l'editeur (~) : ModelContextProtocol.StartServer" -ForegroundColor Yellow
        }
    }

    'clean' {
        if (Test-EditorRunning) { Write-Host '[ue] Ferme l''editeur avant clean.' -ForegroundColor Red; exit 1 }
        foreach ($d in 'Binaries', 'Intermediate') {
            $p = Join-Path $ProjectDir $d
            if (Test-Path $p) { Remove-Item -Recurse -Force $p; Write-Host "[ue] supprime $p" }
        }
    }

    'profile' {
        # Lance le jeu en standalone (non cooke), capture $Frames frames au CSV profiler puis quitte
        $csvDir = Join-Path $ProjectDir 'Saved\Profiling\CSV'
        $before = @(Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | ForEach-Object FullName)
        $gameArgs = @("`"$Uproject`"", $Map, '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-nosplash', '-unattended',
                      "-csvCaptureFrames=$Frames", '-csvGpuStats', '-ExitAfterCsvProfiling')
        Write-Host "[ue] Capture de $Frames frames sur $Map ..."
        Start-Process $EditorExe -ArgumentList $gameArgs -Wait
        $csv = Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue |
            Where-Object { $before -notcontains $_.FullName } | Sort-Object LastWriteTime | Select-Object -Last 1
        if (-not $csv) { Write-Host '[ue] Aucun CSV produit (jeu ferme avant la fin de capture ?).' -ForegroundColor Red; exit 1 }
        Write-Host "[ue] CSV : $($csv.FullName)"
        python (Join-Path $PSScriptRoot 'perf_summary.py') $csv.FullName
        exit $LASTEXITCODE
    }

    'package' {
        if (Test-EditorRunning) {
            Write-Host '[ue] Editeur partage ouvert : coordonner sa fermeture avec son proprietaire avant packaging.'
            exit 3
        }
        $heavy = Get-CimInstance Win32_Process | Where-Object {
            $_.Name -match '^(UnrealEditor-Cmd|UnrealBuildTool|AutomationTool)\.exe$' -or
            ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
        }
        if ($heavy) {
            Write-Host '[ue] Un processus Unreal lourd est actif : attendre avant packaging.'
            exit 3
        }
        $drive = New-Object System.IO.DriveInfo([System.IO.Path]::GetPathRoot($Root))
        if ($drive.AvailableFreeSpace -le 15GB) {
            Write-Host '[ue] Packaging refuse : plus de 15 Gio libres requis.'
            exit 3
        }
        $metadata = Join-Path $PSScriptRoot 'build_metadata.py'
        python $metadata stamp --version $Version
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        $out = Join-Path $Root 'build\package'
        # Never replace a prior package: a unique version owns its archive directory.
        $buildInfo = Get-Content -Raw (Join-Path $ProjectDir 'Saved\BuildMetadata\build.json') | ConvertFrom-Json
        $out = Join-Path $out $buildInfo.version
        $crashArgs = if ($Config -eq 'Development') { @('-CrashReporter') } else { @() }
        & $RunUAT BuildCookRun "-project=$Uproject" -platform=Win64 "-clientconfig=$Config" `
            -build -cook -stage -pak -iostore -nodebuginfo -archive "-archivedirectory=$out" -noP4 -utf8output -unattended @crashArgs
        $code = $LASTEXITCODE
        if ($code -eq 0) {
            python $metadata archive --package $out
            $code = $LASTEXITCODE
            if ($code -eq 0) { Write-Host "[ue] Package : $out ; symboles hors package dans Saved/Symbols" }
        }
        exit $code
    }
}
} finally {
    if ($TookLock) { python $LockScript release --owner $LockOwner | Out-Null }
}

rem @echo off
if /i '%1' == 'fpc' (
  set OUTDIR="..\..\bin10\Release\Plugins"
) else if /i '%1' == 'fpc64' (
  set OUTDIR="..\..\bin10\Release64\Plugins"
)
if not exist %OUTDIR% mkdir %OUTDIR%
md tmp
set myopts=-O3 -Xs -Sd -dMiranda -FE.\tmp -FU.\tmp -FE%OUTDIR% -Fi..\Utils.pas -Fi..\ExternalAPI\delphi -Fu..\Utils.pas -Fu..\..\include\delphi -Fu..\ExternalAPI\delphi
set dprname=Watrack.dpr

rem brcc32.exe res\watrack.rc         -fores\watrack.res
rem brcc32.exe lastfm\lastfm.rc       -folastfm\lastfm.res
rem brcc32.exe myshows\myshows.rc     -fomyshows\myshows.res
rem brcc32.exe players\mradio.rc      -foplayers\mradio.res
rem brcc32.exe kolframe\frm.rc        -fokolframe\frm.res
rem brcc32.exe popup\popup.rc         -fopopup\popup.res
rem brcc32.exe proto\proto.rc         -foproto\proto.res
rem brcc32.exe stat\stat.rc           -fostat\stat.res
rem brcc32.exe status\status.rc       -fostatus\status.res
rem brcc32.exe templates\templates.rc -fotemplates\templates.res

if /i '%1' == 'fpc' (
  fpc.exe %myopts% %dprname% %2 %3 %4 %5 %6 %7 %8 %9
) else if /i '%1' == 'fpc64' (
  ppcrossx64.exe %myopts% %dprname% %2 %3 %4 %5 %6 %7 %8 %9
)

del /Q tmp\*
rd tmp
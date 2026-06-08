#define AppName "NoSteamWebHelper"
#define AppVersion "1.0.0"
#define AppPublisher "NoSteamWebHelper"
#define OutputBaseName "NoSteamWebHelperSetup"

[Setup]
AppId={{B950978D-64D1-42F8-9A99-4BB760D0979B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={code:GetDefaultSteamDir}
DisableProgramGroupPage=yes
OutputDir=bin
OutputBaseFilename={#OutputBaseName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName}

[Files]
Source: "..\src\bin\umpdc.dll"; DestDir: "{app}"; DestName: "umpdc.dll"; Flags: ignoreversion; BeforeInstall: BackupExistingDll

[Code]
const
  SteamKey = 'Software\Valve\Steam';
  DllName = 'umpdc.dll';
  BackupName = 'umpdc.dll.bak';

function DirContainsSteam(const Dir: String): Boolean;
begin
  Result := FileExists(AddBackslash(Dir) + 'steam.exe');
end;

function ParentDirOfFile(const FileName: String): String;
begin
  Result := RemoveBackslashUnlessRoot(ExtractFileDir(FileName));
end;

function ReadSteamDirFromRegistry(var Dir: String): Boolean;
var
  Value: String;
begin
  Result := False;

  if RegQueryStringValue(HKCU, SteamKey, 'SteamExe', Value) then
  begin
    Dir := ParentDirOfFile(Value);
    if DirContainsSteam(Dir) then
    begin
      Result := True;
      Exit;
    end;
  end;

  if RegQueryStringValue(HKCU, SteamKey, 'SteamPath', Value) then
  begin
    Dir := RemoveBackslashUnlessRoot(Value);
    if DirContainsSteam(Dir) then
    begin
      Result := True;
      Exit;
    end;
  end;

  if RegQueryStringValue(HKLM32, SteamKey, 'InstallPath', Value) then
  begin
    Dir := RemoveBackslashUnlessRoot(Value);
    if DirContainsSteam(Dir) then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function GetDefaultSteamDir(Param: String): String;
var
  Dir: String;
begin
  if ReadSteamDirFromRegistry(Dir) then
    Result := Dir
  else
    Result := ExpandConstant('{commonpf32}\Steam');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  if CurPageID = wpSelectDir then
  begin
    if not DirContainsSteam(WizardDirValue) then
    begin
      MsgBox('Please select the Steam directory that contains steam.exe.', mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure InitializeWizard;
begin
  MsgBox('Please close Steam before installing NoSteamWebHelper.', mbInformation, MB_OK);
end;

procedure BackupExistingDll;
var
  DllPath: String;
  BackupPath: String;
begin
  DllPath := ExpandConstant('{app}\') + DllName;
  BackupPath := ExpandConstant('{app}\') + BackupName;

  if FileExists(DllPath) and not FileExists(BackupPath) then
    CopyFile(DllPath, BackupPath, False);
end;

function InitializeUninstall: Boolean;
begin
  Result := True;
  MsgBox('Please close Steam before uninstalling NoSteamWebHelper.', mbInformation, MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DllPath: String;
  BackupPath: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    DllPath := ExpandConstant('{app}\') + DllName;
    BackupPath := ExpandConstant('{app}\') + BackupName;

    if FileExists(DllPath) then
      DeleteFile(DllPath);

    if FileExists(BackupPath) then
      RenameFile(BackupPath, DllPath);
  end;
end;

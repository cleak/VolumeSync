Section "PostInstall"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "VolumeSync" "$INSTDIR\VolumeSync.exe"

SectionEnd
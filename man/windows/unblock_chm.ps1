$chm = "C:\Users\dmitr\src\nls\man\nls.chm"
if (Test-Path $chm) {
  Unblock-File $chm
  Write-Host "Unblocked $chm"
} else {
  Write-Host "Not found: $chm"
}

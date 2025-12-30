; This file is included into the generated NSIS script by CPack

!include "WinMessages.nsh"

Var StartMenuGroup

; Override the default PATH page initialization
Function AddToPathPagePre
  ; Call the original macro to build the page
  !insertmacro MUI_HEADER_TEXT "Add to PATH" "Choose whether to add MyApp to PATH"

  ; Force "all users" radio button to be checked
  ; The control IDs come from the stock CPack NSIS script
  ; 1201 = "current user", 1202 = "all users"
  SendMessage $HWNDPARENT ${BM_SETCHECK} ${BST_UNCHECKED} 0 /CONTROL 1201
  SendMessage $HWNDPARENT ${BM_SETCHECK} ${BST_CHECKED}   0 /CONTROL 1202
FunctionEnd

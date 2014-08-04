$iesid = "S-1-15-3-4096"
$aapsid = "APPLICATION PACKAGE AUTHORITY\ALL APPLICATION PACKAGES"

ForEach($file in (Get-ChildItem -recurse -Path 'C:\users\tyranid\appdata\')) {
   $acl = Get-Acl -path $file.PSPath
   ForEach($ace in $acl.Access) {
      If($ace.FileSystemRights -eq 
         [Security.AccessControl.FileSystemRights]::FullControl -and 
            $ace.IdentityReference.Value -in $iesid, $aapsid) {
               Write-Output $file.PSPath
      }  
   }
}

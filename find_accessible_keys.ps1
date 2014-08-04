$iesid = "S-1-15-3-4096"
$aapsid = "APPLICATION PACKAGE AUTHORITY\ALL APPLICATION PACKAGES"

ForEach($key in (Get-ChildItem -recurse -Path 'HKCU:\')) {
   $acl = Get-Acl -path $key.PSPath
   ForEach($ace in $acl.Access) {
      If($ace.RegistryRights -eq 
         [Security.AccessControl.RegistryRights]::FullControl -and 
            $ace.IdentityReference.Value -in $iesid, $aapsid) {
               Write-Output $key.PSPath
      }  
   }
}

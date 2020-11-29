# Copyright (c) 2020 The Orbit Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

$conan = Get-Command -ErrorAction Ignore conan

Set-Variable preferred_conan_version -option ReadOnly -value "1.31.4"
Set-Variable conan_version_major_required -option ReadOnly -value 1
Set-Variable conan_version_minor_min -option ReadOnly -value 31

if (!$conan) {
  Write-Host "Conan not found. Trying to install it via python-pip..."
	
  $pip3 = Get-Command -ErrorAction Ignore pip3
	
  if(!$pip3) {	
    Write-Error -ErrorAction Stop @"
It seems you don't have Python3 installed (pip3.exe not found).
Please install Python or make it available in the path.
Alternatively you could also just install conan manually and make
it available in the path.
"@
  }

  & $pip3.Path install conan==$preferred_conan_version
  if ($LastExitCode -ne 0) {
    Throw "Error while installing conan via pip3."
  }
    
  $conan = Get-Command -ErrorAction Ignore conan
  if (!$conan) {
    Write-Error -ErrorAction Stop @"
It seems we installed conan sucessfully, but it is not available
in the path. Please ensure that your Python user-executable folder is
in the path and call this script again.
You can call 'pip3 show -f conan' to figure out where conan.exe was placed.
"@
  }
} else {
  Write-Host "Conan found. Checking version..."

  $conan_version = (& $conan.Path --version).split(" ")[2]
  $conan_version_major = $conan_version.split(".")[0] -as [int]
  $conan_version_minor = $conan_version.split(".")[1] -as [int]


  if ($conan_version_major -eq $conan_version_major_required -and $conan_version_minor -lt $conan_version_minor_min) {
    Write-Host "Your conan version $conan_version is too old. Let's try to update it."

    Try {
      $pip3 = Get-Command pip3
      & $pip3.Path install --upgrade conan==$preferred_conan_version
    } Catch {
      Throw "Error while upgrading conan via pip3. Probably you have conan installed differently." + 
            " Please manually update conan to a at least version $conan_version_major_required.$conan_version_minor_min."
    }

    Write-Host "Successfully updated conan!"
  } else {
    Write-Host "Found conan version $conan_version. That fulfills the requirements!"
  }
}

# Install conan config
& "$PSScriptRoot\third_party\conan\configs\install.ps1"

# Start build
if ($args) {
  & "$PSScriptRoot\build.ps1" $args
} elseif (& $conan.Path remote list | Select-String -NotMatch "Disabled:" | Select-String "artifactory:") {
  & "$PSScriptRoot\build.ps1" "default_relwithdebinfo" "ggp_relwithdebinfo"
} else {
  & "$PSScriptRoot\build.ps1"
}

// This file is part of IE11SandboxEsacapes.

// IE11SandboxEscapes is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// IE11SandboxEscapes is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with IE11SandboxEscapes.  If not, see <http://www.gnu.org/licenses/>.

using Microsoft.Win32;
using System;
using System.IO;

namespace DumpElevationPolicy
{
    class ElevationPolicyEntry
    {
        public ElevationPolicyType Policy { get; private set; }
        public string FullPath { get; private set; }
        public Guid Uuid { get; private set; }
        public bool Class { get; private set; }

        private static string HandleNulTerminate(string s)
        {
            int index = s.IndexOf('\0');
            if (index >= 0)
            {
                return s.Substring(0, index);
            }
            else
            {
                return s;
            }
        }

        public ElevationPolicyEntry(Guid uuid, RegistryKey rootKey)
        {
            Uuid = uuid;
            LoadFromRegistry(rootKey);
        }

        private void LoadFromRegistry(RegistryKey key)
        {
            object policyValue = key.GetValue("Policy", 0);

            if (policyValue != null)
            {
                Policy = (ElevationPolicyType)Enum.ToObject(typeof(ElevationPolicyType), key.GetValue("Policy", 0));
            }

            string clsid = (string)key.GetValue("CLSID");

            if (clsid != null)
            {
                Guid cls;

                if (Guid.TryParse(clsid, out cls))
                {
                    FullPath = "CLSID:"+cls.ToString();
                    Class = true;
                }
            }
            else
            {
                string appName = (string)key.GetValue("AppName", null);
                string appPath = (string)key.GetValue("AppPath");

                if ((appName != null) && (appPath != null))
                {
                    try
                    {
                        FullPath = Path.Combine(HandleNulTerminate(appPath), HandleNulTerminate(appName));
                    }
                    catch (ArgumentException)
                    {
                    }
                }
            }
        }
    }

    enum ElevationPolicyType
    {
        NoRun = 0,
        RunAtCurrent = 1,
        RunAfterPrompt = 2,
        RunAtMedium = 3,
    }
}

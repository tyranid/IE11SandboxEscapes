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
using NDesk.Options;
using System;
using System.Collections.Generic;
using System.Linq;

namespace DumpElevationPolicy
{
    class Program
    {
        enum DumpType
        {
            All,
            Processes,
            Classes,
        }

        static bool show_help = false;
        static HashSet<ElevationPolicyType> policy = new HashSet<ElevationPolicyType>();
        static bool print_path = false;
        static DumpType dump_type = DumpType.All;

        static List<ElevationPolicyEntry> LoadElevationPolicy(RegistryKey rootKey)
        {
            List<ElevationPolicyEntry> ret = new List<ElevationPolicyEntry>();
            using (RegistryKey key = rootKey.OpenSubKey("SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\ElevationPolicy"))
            {
                if (key != null)
                {
                    string[] subkeys = key.GetSubKeyNames();
                    foreach (string s in subkeys)
                    {
                        Guid g;

                        if (Guid.TryParse(s, out g))
                        {
                            using (RegistryKey rightsKey = key.OpenSubKey(s))
                            {
                                ElevationPolicyEntry entry = new ElevationPolicyEntry(g, rightsKey);
                                if (!String.IsNullOrWhiteSpace(entry.FullPath))
                                {
                                    ret.Add(entry);
                                }
                            }
                        }
                    }
                }
            }
            return ret;
        }

        static void LoadAndDumpElevationPolicy(string name, RegistryHive hive, RegistryView view)
        {
            try
            {
                using (RegistryKey key = RegistryKey.OpenBaseKey(hive, view))
                {
                    if (key != null)
                    {
                        IEnumerable<ElevationPolicyEntry> entries = LoadElevationPolicy(key);
                        if (policy.Count > 0)
                        {
                            entries = entries.Where(e => policy.Contains(e.Policy));
                        }

                        switch (dump_type)
                        {
                            case DumpType.Classes:
                                entries = entries.Where(e => e.Class);
                                break;
                            case DumpType.Processes:
                                entries = entries.Where(e => !e.Class);
                                break;
                        }

                        if (entries.Count() > 0)
                        {
                            Console.WriteLine(name);
                            foreach (ElevationPolicyEntry entry in entries)
                            {
                                if(!print_path)
                                {
                                    Console.WriteLine("UUID: {0}", entry.Uuid);
                                    Console.WriteLine("FullPath: {0}", entry.FullPath);
                                    Console.WriteLine("Policy: {0}", entry.Policy);
                                }
                                else
                                {
                                    Console.WriteLine(entry.FullPath);
                                }
                            }

                            Console.WriteLine();
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error dumping {0} - {1}", name, ex.Message);
            }
        }

        static void ShowHelp(OptionSet p)
        {
            Console.WriteLine("Usage: DumpElevationPolicy [OPTIONS]");
            Console.WriteLine("Dump Internet Explorer Low-Rights elevation policy");
            Console.WriteLine("Options:");
            p.WriteOptionDescriptions(Console.Out);
        }

        static string EnumAsString(Type enumType)
        {
            return String.Join(",", Enum.GetNames(enumType).Select(n => n.ToLower()));
        }

        static void AddEnum<T>(HashSet<T> list, string name) where T : struct
        {
            T result;

            if (Enum.TryParse(name, true, out result))
            {
                list.Add(result);
            }
        }

        static void Main(string[] args)
        {
            var opts = new OptionSet () {
   	            { "h|?|help", "Display this help",  v => show_help = v != null },
                { "f", "Print only the path", v => print_path = v != null },
                { "p|policy=", String.Format("Only show entries with specified policy [{0}]", EnumAsString(typeof(ElevationPolicyType)))
                , v => AddEnum(policy, v) },
                { "d|dump=", String.Format("Specify the dump type [{0}]", EnumAsString(typeof(DumpType))), v => Enum.TryParse(v, true, out dump_type) },
           };
           List<string> extra = opts.Parse (args);

           if (show_help)
           {
               ShowHelp(opts);
           }
           else
           {
               LoadAndDumpElevationPolicy("Local Machine Policy", RegistryHive.LocalMachine, RegistryView.Default);
               LoadAndDumpElevationPolicy("Current User Policy", RegistryHive.CurrentUser, RegistryView.Default);
               if (Environment.Is64BitProcess)
               {
                   LoadAndDumpElevationPolicy("Local Machine Policy (32bit)", RegistryHive.LocalMachine, RegistryView.Registry32);
                   LoadAndDumpElevationPolicy("Current User Policy (32bit)", RegistryHive.CurrentUser, RegistryView.Registry32);
               }
           }
        }
    }
}

package test_runner

import (
	"gopkg.in/ini.v1"
	"strings"
	"testing"
	"time"
)

func expectedUpgradePolicy() string {
	if *distroName == "Ubuntu" {
		return "lts"
	}
	if strings.HasPrefix(*distroName, "Ubuntu") && strings.HasSuffix(*distroName, "LTS") {
		return "never"
	}
	// Preview and Dev
	return "normal"
}

func TestBasicSetup(t *testing.T) {
	// Wrapping testing.T to get the friendly additions to its API as declared in `wsl_tester.go`.
	tester := WslTester(t)

	// Invoke the launcher as if the user did: `<launcher>.exe install --ui=gui`
	outStr := tester.AssertLauncherCommand("install --ui=gui") // without install the launcher will run bash.
	if len(outStr) == 0 {
		tester.Fatal("Failed to install the distro: No output produced.")
	}

	// After completing the setup, the OOBE must have created a new user and the distro launcher must have set it as the default.
	{
		outputStr := tester.AssertWslCommand("whoami")
		notRoot := strings.Fields(outputStr)[0]
		if notRoot == "root" {
			tester.Logf("%s", outputStr)
			tester.Fatal("Default user should not be root.")
		}
	}

	// Subiquity either installs or marks for installation the relevant language packs for the chosen language.
	{
		outputStr := tester.AssertWslCommand("apt-mark", "showinstall", "language-pack\\*")
		packs := strings.Fields(outputStr)
		tester.Logf("%s", outputStr) // I'd like to see what packs were installed in the end, just in case.
		if len(packs) == 0 {
			tester.Fatal("At least one language pack should have been installed or marked for installation, but apt-mark command output is empty.")
		}
	}

	// Proper release. Ensures the right tarball was used as rootfs
	{
		expectedRelease := func() string {
			if *distroName == "Ubuntu-Preview" || *distroName == "UbuntuDev.WslID.Dev" {
				return "Ubuntu Kinetic Kudu (development branch)"
			}
			if *distroName == "Ubuntu" {
				return "Ubuntu 22.04.1 LTS"
			}
			if *distroName == "Ubuntu22.04LTS" {
				return "Ubuntu 22.04.1 LTS"
			}
			if *distroName == "Ubuntu20.04LTS" {
				return "Ubuntu 20.04.5 LTS"
			}
			if *distroName == "Ubuntu18.04LTS" {
				return "Ubuntu 18.04.6 LTS"
			}
			return ""
		}()
		if len(expectedRelease) == 0 {
			tester.Logf("Unknown Ubuntu release corresponding to distro name '%s'", *distroName)
			tester.Fatal("Unexpected value provided via --distro-name")
		}
		outputStr := tester.AssertWslCommand("cat", "/etc/os-release")
		cfg, err := ini.Load([]byte(outputStr))
		if err != nil {
			tester.Logf("Contents of /etc/os-release:\n%s", outputStr)
			tester.Fatal("Failed to parse ini file")
		}

		release, err := cfg.Section(ini.DefaultSection).GetKey("PRETTY_NAME")
		if err != nil {
			tester.Logf("Contents of /etc/os-release:\n%s", outputStr)
			tester.Fatal("Failed to find PRETTY_NAME")
		}

		if release.String() != expectedRelease {
			tester.Logf("Contents of /etc/os-release:\n%s", outputStr)
			tester.Logf("Parsed release:   %s", release.String())
			tester.Logf("Expected release: %s", expectedRelease)
			tester.Fatal("Unexpected release string")
		}
	}

	// Systemd enabled.
	{
		getSystemdStatus := func() string {
			outputStr := tester.AssertWslCommand("bash", "-ec", "systemctl is-system-running || exit 0")
			return strings.TrimSpace(outputStr)
		}

		status := getSystemdStatus()
		for status == "starting" {
			time.Sleep(1 * time.Second)
			status = getSystemdStatus()
		}

		if status != "degraded" && status != "running" {
			tester.Logf("%s", status)
			tester.Fatal("Systemd should have been enabled")
		}
	}

	// Sysusers service fix
	{
		tester.AssertWslCommand("systemctl", "status", "systemd-sysusers.service")
	}

	// ---------------- Reboot with launcher -----------------
	tester.AssertOsCommand("wsl.exe", "-t", *distroName)
	tester.AssertLauncherCommand("run echo Hello")

	// Upgrade policy
	{
		outputStr := tester.AssertWslCommand("cat", "/etc/update-manager/release-upgrades")
		cfg, err := ini.Load([]byte(outputStr))
		if err != nil {
			tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
			tester.Fatal("Failed to parse ini file")
		}

		section, err := cfg.GetSection("DEFAULT")
		if err != nil {
			tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
			tester.Fatal("Failed to find section DEFAULT in /etc/update-manager/release-upgrades")
		}
		value, err := section.GetKey("Prompt")
		if err != nil {
			tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
			tester.Fatal("Failed to find key 'Prompt' in DEFAULT section of /etc/update-manager/release-upgrades")
		}
		if value.String() != expectedUpgradePolicy() {
			tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
			tester.Logf("Parsed policy: %s", value.String())
			tester.Logf("Expected policy: %s", expectedUpgradePolicy())
			tester.Fatal("Wrong upgrade policy")
		}

		release_upgrades_date := tester.AssertWslCommand("date", "-r", "/etc/update-manager/release-upgrades")

		// ---------------- Reboot with launcher -----------------
		tester.AssertOsCommand("wsl.exe", "-t", *distroName)
		tester.AssertLauncherCommand("run echo Hello")

		new_date := tester.AssertWslCommand("date", "-r", "/etc/update-manager/release-upgrades")
		if release_upgrades_date != new_date {
			tester.Logf("Launcher is modifying release upgrade file more than once")
		}
	}

}

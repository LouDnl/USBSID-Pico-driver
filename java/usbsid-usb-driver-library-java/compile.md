# Patched libusb4java native libs (Windows)

Stock `org.usb4java:libusb4java:1.3.0` win32 jars from Maven Central bundle a
statically-linked libusb core compiled ~Oct 2018 (pre libusb 1.0.24). That core
lacks `WinUsb_GetAssociatedInterface` support, so claiming a second interface of
an IAD-grouped composite function (our CDC control+data) fails with
`LIBUSB_ERROR_NOT_SUPPORTED` (-12) on Windows — even though native C apps linking
a modern system libusb (1.0.26/1.0.27) claim it fine.

`lib/libusb4java-1.3.0-win32-x86-64.jar` and `lib/libusb4java-1.3.0-win32-x86.jar`
contain a patched/rebuilt `libusb4java.dll` linked against a modern libusb core.
They must be installed into `~/.m2` to **override** the stock Central jars before
`mvn clean install` of this project, otherwise the old DLL gets pulled in:

```shell
mvn -q -o install:install-file -Dfile=lib/libusb4java-1.3.0-win32-x86-64.jar \
  -DgroupId=org.usb4java -DartifactId=libusb4java -Dversion=1.3.0 \
  -Dclassifier=win32-x86-64 -Dpackaging=jar

mvn -q -o install:install-file -Dfile=lib/libusb4java-1.3.0-win32-x86.jar \
  -DgroupId=org.usb4java -DartifactId=libusb4java -Dversion=1.3.0 \
  -Dclassifier=win32-x86 -Dpackaging=jar
```

Verify the override took (patched DLL is ~436736 bytes, stock is 385024):

```shell
unzip -p ~/.m2/repository/org/usb4java/libusb4java/1.3.0/libusb4java-1.3.0-win32-x86-64.jar \
  org/usb4java/win32-x86-64/libusb4java.dll | wc -c
```

Only needs doing once per `~/.m2` (e.g. after a fresh checkout or `mvn dependency:purge-local-repository`).

# Tips

```shell
# Compile and install into .m2/repository/usbsid/
mvn clean install

# Replace jsidplay2 driver
## MAIN
VER=1.1;
cp /home/loud/.m2/repository/usbsid/usbsid-usb-driver-library-java/$VER/usbsid-usb-driver-library-java-$VER.jar /mnt/loud/Code/Development/c64/sidplaytrack/jsidplay/svn.jsidplay2-code/jsidplay2/target/standalone/usbsid-usb-driver-library-java-$VER.jar
## THINKPAD
VER=1.1;
cp /home/loud/.m2/repository/usbsid/usbsid-usb-driver-library-java/$VER/usbsid-usb-driver-library-java-$VER.jar /home/loud/Development/c64/svn.jsidplay2-code/jsidplay2/target/standalone/usbsid-usb-driver-library-java-$VER.jar

# Or in one line
## MAIN
mvn clean install && cp target/usbsid-usb-driver-library-java-1.0.jar ~/Development/c64/sidplaytrack/jsidplay/svn.jsidplay2-code/jsidplay2/target/standalone/
## THINKPAD
mvn clean install && cp target/usbsid-usb-driver-library-java-1.0.jar /home/loud/Development/c64/svn.jsidplay2-code/jsidplay2/target/standalone/usbsid-usb-driver-library-java-1.0.jar
```

# Generate classpath
https://maven.apache.org/guides/introduction/introduction-to-dependency-mechanism.html#Dependency_Scope
https://stackoverflow.com/questions/16655010/in-maven-how-output-the-classpath-being-used
```shell
mvn dependency:build-classpath -Dmdep.includeScope=runtime -Dmdep.outputFile=cp.txt
  -Dmdep.includeScope=compile
  -Dmdep.includeScope=runtime
```

# Jsidplay2 build and test
```shell
# COMPILE
export PATH=$PATH:/opt/apache-maven-3.9.10/bin
mvn clean install

# SHELL FILE
cd target/standalone && cp ../deploy/*.sh . ; chmod +x *.sh && ./jsidplay2.sh -E USBSID && cd ../..
## MAIN
cd target/standalone && cp ../deploy/*.sh . ; chmod +x *.sh && ./jsidplay2-console.sh -E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/digitunes/Coma_Light_13_tune_4.sid  && cd ../..
## THINKPAD
cd target/standalone && cp ../deploy/*.sh . ; chmod +x *.sh && ./jsidplay2.sh -E USBSID /home/loud/Development/c64/RETROCOLLECTION/SID-prgs/Commando.prg && cd ../..

# JAVA
mvn exec:java -Dexec.mainClass=ui.JSidPlay2Main -Dexec.args="-E USBSID"
## THINKPAD UI
mvn exec:java -Dexec.mainClass=ui.JSidPlay2Main -Dexec.args="-E USBSID /home/loud/Development/c64/RETROCOLLECTION/SID-prgs/Hypersonic_Lovers_[8580].prg"
mvn exec:java -Dexec.mainClass=ui.JSidPlay2Main -Dexec.args="-E USBSID /home/loud/Development/c64/RETROCOLLECTION/SID-prgs/Supremacy.prg"
## THINKPAD CLI
mvn exec:java -Dexec.mainClass=sidplay.ConsolePlayer -Dexec.args="-E USBSID /home/loud/Development/c64/RETROCOLLECTION/SID-prgs/Supremacy.prg"
mvn exec:java -Dexec.mainClass=sidplay.ConsolePlayer -Dexec.args="-E USBSID /home/loud/Development/c64/RETROCOLLECTION/SID-prgs/Hypersonic_Lovers_[8580].prg"
```

# Jsidplay2 compile and test
```shell
mvn compile

# MAIN
mvn exec:java -Dexec.mainClass=sidplay.ConsolePlayer -Dexec.args="-E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/Commando.sid"

mvn exec:java -Dexec.mainClass=sidplay.ConsolePlayer -Dexec.args="-E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/digitunes/Afterburner.sid"

mvn exec:java -Dexec.mainClass=sidplay.ConsolePlayer -Dexec.args="-E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/digitunes/Coma_Light_13_tune_4.sid"

mvn -q exec:exec -Dexec.executable=java -Dexec.args="-cp %classpath sidplay.ConsolePlayer -E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/Commando.sid"

mvn -q exec:exec -Dexec.executable=java -Dexec.args="-cp %classpath sidplay.ConsolePlayer -E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/digitunes/Coma_Light_13_tune_4.sid"
```
# Debug
https://stackoverflow.com/questions/28752835/is-possible-have-byte-from-0-to-255-in-java
https://stackoverflow.com/questions/2935375/debugging-in-maven
```shell
# with suspend
MAVEN_OPTS="-Xdebug -Xnoagent -Djava.compiler=NONE -Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=8000" ; mvnDebug exec:java -Dexec.mainClass=sidplay.ConsolePlayer -Dexec.args="-E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/Commando.sid"

mvn exec:exec -Dexec.executable="java" -Dexec.args="-classpath %classpath -Xdebug -Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=8000 sidplay.ConsolePlayer -E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/Commando.sid"

# without suspend
MAVEN_OPTS="-Xdebug -Xnoagent -Djava.compiler=NONE -Xrunjdwp:transport=dt_socket,server=y,suspend=n,address=8000" ; mvnDebug exec:java -Dexec.mainClass=sidplay.ConsolePlayer -Dexec.args="-E USBSID /mnt/loud/Code/Development/pi/USBSID-Pico-dev/private-repo/code-usbsid-related/configtool_webusb/SID/Commando.sid"

mvn exec:exec -Dexec.executable="java" -Dexec.args="-classpath %classpath -Xdebug -Xrunjdwp:transport=dt_socket,server=y,suspend=n,address=1044 sidplay.ConsolePlayer"


```

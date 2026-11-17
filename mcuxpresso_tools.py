# Usa as ferramentas instaladas pelo MCUXpresso Installer no build/upload:
#
# - arm-none-eabi-size: o Smart App Control do Windows bloqueia o do
#   toolchain-gccarmnoneeabi@1.80201 do PlatformIO, e o "checkprogsize" roda
#   antes de todo upload. Usa o do Arm GNU Toolchain em ~/.mcuxpressotools.
# - g++, link e objcopy: em 2026-11-09 o Device Guard passou a bloquear tambem
#   o g++ ("foi bloqueado pela politica do Device Guard") e o collect2/ld do
#   PlatformIO ("CreateProcess: No such file or directory"). O Zephyr do
#   PlatformIO compila um __dummy.cpp e linka em todo build. Os tres vem do
#   mesmo Arm GNU Toolchain; o codigo C continua no gcc 8.2 do PlatformIO.
#   O ar/ranlib do PlatformIO avisam "Unable to load DLL" (e o plugin de LTO
#   bloqueado), mas o .a sai integro: e aviso, nao erro.
# - LinkServer: o Upload grava o ELF via SWD pelo LinkServer mais novo em
#   C:/NXP, entao atualizar o LinkServer pelo installer nao quebra o upload.
import glob
import os
import re

Import("env")

LINKSERVER_DEVICE = "MKL25Z4:FRDM-KL25Z"


def mais_novo(padrao, regex_versao):
    def versao(caminho):
        m = re.search(regex_versao, caminho.replace("\\", "/"))
        return tuple(int(x) for x in m.group(1).split(".")) if m else ()
    caminhos = glob.glob(padrao)
    return max(caminhos, key=versao) if caminhos else None


size = mais_novo(
    os.path.join(os.path.expanduser("~"), ".mcuxpressotools",
                 "arm-gnu-toolchain-*", "bin", "arm-none-eabi-size.exe"),
    r"arm-gnu-toolchain-(\d+(?:\.\d+)*)")
if size:
    bindir = os.path.dirname(size)

    def ferramenta(nome):
        return os.path.join(bindir, "arm-none-eabi-%s.exe" % nome)

    # Do toolchain do PlatformIO so gcc/cc1/as passam pelo Device Guard: o
    # C continua compilando nele. g++, link (collect2/ld) e objcopy vem do Arm
    # GNU Toolchain - binutils novo linka objeto de gcc 8 sem problema.
    env.Replace(SIZETOOL=size,
                CXX=ferramenta("g++"),
                LINK=ferramenta("gcc"),
                OBJCOPY=ferramenta("objcopy"))

    # O builder do Zephyr compila cada grupo em um default_env.Clone()
    # criado ANTES deste script (post:), entao o CXX trocado acima nao alcanca
    # o __dummy.cpp: troca tambem no env do proprio no.
    dummy = env.File(os.path.join(
        "$BUILD_DIR", "_pio___bare_module", "modules", "_bare_module",
        "__dummy.cpp.o"))
    if dummy.has_builder():
        dummy.get_env().Replace(CXX=ferramenta("g++"))
else:
    print("AVISO: arm-none-eabi-size do MCUXpresso nao encontrado em "
          "~/.mcuxpressotools; usando o do PlatformIO (pode ser bloqueado).")

linkserver = mais_novo("C:/NXP/LinkServer_*/LinkServer.exe",
                       r"LinkServer_(\d+(?:\.\d+)*)/")
if linkserver:
    env.Replace(UPLOADCMD='"%s" flash %s load "$PROGPATH"'
                % (linkserver, LINKSERVER_DEVICE))
else:
    print("AVISO: LinkServer nao encontrado em C:/NXP; instale-o pelo "
          "MCUXpresso Installer para o Upload funcionar.")

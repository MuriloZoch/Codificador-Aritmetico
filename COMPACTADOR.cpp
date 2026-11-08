#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <iostream>
#include <string>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "TABELA.hpp" // TABELA COM AS FREQUENCIAS QUE CADA SIMBOLO MAIS APARECE NA LINGUA PORTUGUESA

const uint32_t TOPO = 0xFFFFFFFF; // QUASE 1
const uint32_t METADE = 0x80000000; // 0,5
const uint32_t QUARTO = 0x40000000; // 0,25
const uint32_t TRES_QUARTOS = 0xC0000000; // 0,75

class Modelo {
public:
    virtual ~Modelo() = default;
    virtual uint32_t inicio(unsigned char b) = 0; // FATIA INICIAL DE CADA SIMBOLO (F)
    virtual uint32_t fim(unsigned char b) = 0; // FATIA FINAL DE CADA SIMBOLO (F + p)
    virtual uint32_t total() = 0; // SOMA TOTAL DAS FREQUENCIAS DOS SIMBOLOS
    virtual unsigned char simbolo(uint32_t alvo) = 0; //DEVOLVE O SIMBOLO BASEADO NO INTERVALO
    virtual void atualiza(unsigned char) {} // MUDA O TAMANHO DE CADA FATIA (APENAS NO MODELO ADAPTATIVO)
};

class ModeloFixo : public Modelo { // MODELO BASEADO NA TABELA (MELHOR PARA ARQUIVOS CURTOS EM PORTUGUES)
    uint32_t acum[257]; // VETOR QUE ARMAZENA O INICIO DA FATIA DE CADA SIMBOLO
public:
    ModeloFixo() {
        acum[0] = 0;
        for (int b = 0; b < 256; b++) {
            acum[b+1] = acum[b] + FREQ_PT[b]; // O INICIO DA FATIA DO PRÓXIMO SIMBOLO É O INÍCIO DO ATUAL + A FREQUÊNCIA DO ATUAL
        }
    }
    uint32_t inicio(unsigned char b) override { // DEVOLVE O COMEÇO DA FATIA DO SIMBOLO
        return acum[b];
    }
    uint32_t fim(unsigned char b) override { // DEVOLVE O FINAL DA FATIA DE CADA SIMBOLO
        return acum[b + 1];
    }
    uint32_t total() override { // DEVOLVE A SOMA DAS FREQUÊNCIAS ou O FINAL DO VETOR (NESSE CASO = 65536)
        return acum[256];
    }
    unsigned char simbolo(uint32_t alvo) override { // PROCURA PELO SIMBOLO QUE SE ENCAIXA NA FATIA
        int b = 0; // CADA SIMBOLO TEM UM VALOR ASSOCIADO
        while (acum[b + 1] <= alvo) {  // PROCURA b ENQUANTO O ALVO NÃO ESTÁ NA FATIA DESEJADA
            b++;
        }
        return (unsigned char) b;
    }
};

class ModeloAdaptativo : public Modelo { // MODELO PARA ARQUIVOS LONGOS (NÃO PRECISAM OBEDECER UMA LINGUA)
    uint32_t freq[256];
    uint32_t soma;
public:
    ModeloAdaptativo() {
        for (int i = 0; i < 256; i++) { // INICIALMENTE TODOS OS SIMBOLOS TEM A MESMA FREQUÊNCIA = 1
            freq[i] = 1;
        }
        soma = 256;
    }
    uint32_t inicio(unsigned char b) override {
        uint32_t s = 0; // S É O INICIO DA FATIA DE UM SIMBOLO
        for (int i = 0; i < b; i++) {
            s += freq[i]; // PORTANTO, É A SOMA DA FREQUÊNCIA DOS SIMBOLOS ANTERIORES
        }
        return s;
    }
    uint32_t fim(unsigned char b) override { // CALCULA O FINAL DA FATIA DE UM SIMBOLO
        return inicio(b) + freq[b]; 
    }
    uint32_t total() override {
        return soma;
    }
    unsigned char simbolo(uint32_t alvo) override { // PROCURA PELO SIMBOLO CUJA FATIA ESTÁ DENTRO DO INTERVALO DE PROBABILIDADE
        uint32_t s = 0;
        int b = 0;

        while (s + freq[b] <= alvo) {
            s += freq[b];
            b++;
        }

        return (unsigned char)b;
    }
    void atualiza(unsigned char b) override { // ATUALIZA A FREQUENCIA DO SIMBOLO TODA VEZ QUE ELE APARECE
        freq[b] += 32; // 32 GARANTE UM AUMENTO SIGNIFICATIVO NA PROBABILIDADE EM ARQUIVOS CURTOS MAS TAMBÉM NÃO EXTRAPOLA O LIMITE TÃO RAPIDAMENTE
        soma += 32;

        if (soma > 65536) { // QUANDO A SOMA EXTRAPOLA O LIMITE AS FREQUÊNCIAS SÃO DIVIDIDAS PELA METADE, O QUE DA MAIS ESPAÇO E CONSERVA AS PROBABILIDADES DOS SIMBOLOS
            soma = 0;
            for (int i = 0; i < 256; i++) {
                freq[i] = (freq[i] + 1) / 2;
                soma += freq[i];
            }
        }
    }
};

struct Codificador { // O NOSSO CODIFICADOR PRECISA TER UM MINIMO E MAXIMO QUE FUNCIONAM COMO O INTERVALO ALVO DE PROBABILIDADE
    uint32_t L = 0;
    uint32_t H = TOPO;
    int pendentes = 0;
    std::string bits = ""; // E ARMAZENA NOSSA STRING DE BITS COMPACTADOS
};

void emite(Codificador& c, char b) { // ADICIONA OS BITS NA STRING DO CODIFICADOR
    c.bits += b;
    while (c.pendentes > 0) { // BITS PENDENTES OCORREM QUANDO O INTERVALO DA MENSAGEM ACABA EM VOLTA DO INTERVALO ALVO, ASSIM O BIT SEGUINTE SERÁ O OPOSTO DO BIT PENDENTE
        c.bits += (b == '0' ? '1' : '0'); // SE O PRÓXIMO BIT É 1 O BIT PENDENTE É 0 E VICE-VERSA
        c.pendentes--;
    }
}

void renormalizador(Codificador& c) { // AMPLIA O INTERVALO L - H PARA QUE OS ESPAÇOS NÃO SEJAM ESPREMIDOS ATÉ UMA LARGURA PRÓXIMA DE 0
    while(true) {
        if (c.H < METADE) {
            emite(c, '0');
        }
        else if (c.L >= METADE) {
            emite(c, '1');
            c.L -= METADE;
            c.H -= METADE;
        }
        else if (c.L >= QUARTO && c.H < TRES_QUARTOS){
            c.pendentes++;
            c.L -= QUARTO;
            c.H -= QUARTO;
        }
        else break;
        c.L = c.L << 1; // DESLOCA UM BIT PARA ESQUERDA, DESCARTANDO O PRIMEIRO E DANDO ESPAÇO PARA UM NOVO
        c.H = (c.H << 1) | 1; // SIMILAR MAS ADICIONA UM 1 AO INVES DE 0
    }
}

//  COMPACTADOR
void zoomIn(Codificador& c, Modelo* m, unsigned char b){
    uint64_t W = (uint64_t)c.H - c.L + 1; // CALCULA A LARGURA DO INTERVALO ATUAL [L, H]
    c.H = c.L + (uint32_t)(W * m->fim(b) / m->total()) - 1; // CALCULA O NOVO MAXIMO DO INTERVALO BASEADO NO SIMBOLO LIDO
    c.L = c.L + (uint32_t)(W * m->inicio(b) / m->total()); // CALCULA O NOVO MINIMO DO INTERVALO BASEADO NO SIMBOLO LIDO
    renormalizador(c);
    m->atualiza(b);
}

void finaliza(Codificador& c) { // EMITE OS ÚLTIMOS BITS PARA QUE O NÚMERO FIQUE DENTRO DE [L, H]
    c.pendentes++;
    if (c.L < QUARTO) {
        emite(c, '0');
    }
    else {
        emite(c, '1');
    }
}

std::string codificador(const std::string& msg, Modelo* m) { // RODA O LOOP PARA CADA LETRA DA MENSAGEM E RETORNA A STRING EM BINARIO
    Codificador c;

    for (char letra : msg) {
        zoomIn(c, m, letra);
    }
    finaliza(c);
    
    return c.bits;  
}

//DESCOMPACTADOR

struct Decodificador { // ASSIM COMO O COMPACTADOR, O DESCOMPACTADOR PRECISA DOS SEUS PARAMETROS
    uint32_t L = 0; // MINIMO INICIAL DO INTERVALO
    uint32_t H = TOPO; // MAXIMO INICIAL DO INTERVALO
    uint32_t valor = 0; // POSIÇÃO DO NUMERO COMPACTADO DENTRO DO INTERVALO [L, H]
    std::string bits; // É A STRING CODIFICADA QUE SERÁ DESCOMPACTADA
    std::size_t pos = 0; // GUARDA A POSIÇÃO DO ÚLTIMO BIT LIDO
};

int proximo_bit(Decodificador& d) { // SEMPRE QUE A POSIÇÃO DO LEITOR FOR MENOR QUE O TAMANHO DA STRING PASSA PARA O PRÓXIMO NÚMERO
    if (d.pos < d.bits.size()) {
        return d.bits[d.pos++] - '0'; // COMO ESTÁ LENDO UMA STRING O '0' TEM VALOR 48 E O '1' TEM VALOR 49, SUBTRAINDO O '0' (48) O VALOR RETORNADO É SEMPRE 0 OU 1
    }
    return 0;
}

void renormalizador_dec(Decodificador& d) { // ASSIM COMO NO RENORMALIZADOR DO COMPACTADOR ELE AMPLIA O INTERVALO [L, H] PARA QUE NÃO FIQUE MUITO ESTREITO
    while(true) {
        if (d.H < METADE) {}
        else if (d.L >= METADE) {
            d.L -= METADE;
            d.H -= METADE;
            d.valor -= METADE;
        }
        else if (d.L >= QUARTO && d.H < TRES_QUARTOS) {
            d.L -= QUARTO;
            d.H -= QUARTO;
            d.valor -= QUARTO;
        }
        else break;
        
        d.L = d.L << 1;
        d.H = (d.H << 1) | 1;
        d.valor = (d.valor << 1) | proximo_bit(d);
    }
}

char zoomOut(Decodificador& d, Modelo* m) { // FAZ O PROCESSO INVERSO DO ZOOM IN
    uint64_t W = (uint64_t)d.H - d.L + 1; // CALCULA A LARGURA DO INTERVALO ATUAL [L, H]
    uint32_t alvo = (uint32_t)((((uint64_t)d.valor - d.L + 1) * m->total() - 1) / W); // TRANSFORMA O VALOR ALVO DE 32 BITS PARA A ESCALA DO MODELO (65536 NO MODELO FIXO)
    unsigned char b = m-> simbolo(alvo);

    d.H = d.L + (uint32_t)(W * m->fim(b) / m->total()) - 1; // CALCULA O MAXIMO BASEADO NO CARACTER NA NOVA ESCALA
    d.L = d.L + (uint32_t)(W * m->inicio(b) / m->total()); // CALCULA O MINIMO BASEADO NO CARACTER NA NOVA ESCALA

    renormalizador_dec(d);
    m->atualiza(b);
    return (char) b;
}   

std::string decodificador(const std::string& bits, int n, Modelo* m) { // LÊ A STRING EM FATIAS DE 32 BITS PASSANDO DE 1 EM 1
    Decodificador d;
    d.bits = bits;
    for (int i = 0; i < 32; i++) {
        d.valor = (d.valor << 1) | proximo_bit(d);
    }

    std::string texto = "";
    for (int i = 0; i < n; i++) { // RECUPERA CADA SIMBOLO ATÉ CHEGAR NOS n ORIGINAIS
        texto += (char)zoomOut(d, m);
    }

    return texto;
}

std::string ler_arquivo(const std::string& nome) {
    std::ifstream arq(nome, std::ios::binary); // ACESSA O ARQUIVO BRUTO EM BINÁRIO SEM NENHUMA ALTERAÇÃO
    if (!arq) {
        std::cout << "NÃO FOI POSSIVEL ABRIR: " << nome << std::endl; // SE O NOME ESTIVER ERRADO OU O ARQUIVO NÃO EXISTIR ELE DEVOLVE UMA STRING VAZIA
        return "";
    }
    std::stringstream ss; // UMA STRING STREAM É UMA STRING QUE FUNCIONA COMO ARQUIVO, PODEMOS ADICIONAR COISAS UTILIZANDO << COMO COUT
    ss << arq.rdbuf(); // .RDBUF PEGA TODO O CONTEÚDO DO ARQUIVO E COLOCA NA STRING SEM PRECISAR DE LOOP
    return ss.str(); // .STR DEVOLVE SS COMO UMA STRING NORMAL
}

void gravar_arquivo(const std::string& nome, const std::string& dados) {
    std::ofstream arq(nome, std::ios::binary); // CRIA OU ESVAZIA UM ARQUIVO PARA EDIÇÃO
    arq.write(dados.data(), dados.size()); // .DATA INFORMA A POSIÇÃO NA MEMÓRIA DOS CARACTERES E .SIZE INDICA QUANTOS BYTES SERÃO ESCRITOS
}

void gravar_compactado(const std::string& nome, uint64_t n, char modelo, const std::string& bits) {
    std::ofstream arq(nome, std::ios::binary);
    arq.write((const char*)&n, sizeof(n)); // GRAVA O TAMANHO ORIGINAL DO ARQUIVO n
    arq.put(modelo); // ADICIONA UM CARACTER 'f' OU 'a' QUE INDICA O MODELO UTILIZADO
    // JUNTOS SOMAM 9 BYTES NO CABEÇALHO DO ARQUIVO

    uint8_t atual = 0; // O BYTE QUE ESTÁ SENDO MONTADO
    int cont = 0; // QUANTOS BITS ENTRARAM NO BYTE MONTADO
    for (char c : bits) {
        atual = (atual << 1) | (c == '1'); // EMPURRA OS BITS UMA CASA PARA A ESQUERDA E ADICIONA '1' OU '0' NA CASA LIVRE
        cont++;
        if (cont == 8) { // QUANDO O BYTE CHEGA A 8 BITS ELE ESTÁ COMPLETO
            arq.put((char)atual); // ELE É GRAVADO NO ARQUIVO
            atual = 0; // OS CONTADORES SÃO ZERADOS PARA O PRÓXIMO BYTE
            cont = 0;
        }
    }

    if (cont > 0) { 
        arq.put((char)(atual << (8 - cont))); // SE A QUANTIDADE DE BITS NO FINAL NO BYTE NÃO FOR 8 OS BITS SÃO EMPURRADOS PARA A ESQUERDA E O BYTE É COMPLETADO COM ZEROS
    }
}

std::string ler_compactado(const std::string& nome, uint64_t& n, char& modelo) { // DEVOLVEMOS N E MODELO POR REFERENCIA E A STRING PELO RETURN
    std::ifstream arq(nome, std::ios::binary);
    arq.read((char*)&n, sizeof(n)); // INDICA O ENDEREÇO DE N E O TAMANHO DE N EM BYTES
    arq.get(modelo); // LÊ O 9º BYTE QUE INDICA O TIPO DE COMPACTADOR USADO 'f' OU 'a'

    std::string bits = "";
    char byte;
    while (arq.get(byte)) { // LÊ OS BYTES RESTANTES UM POR UM, QUANDO O ARQUIVO ACABA O GET DEVOLVE false
        for (int i= 7; i >= 0; i--) { // A CONTAGEM DE 7 A 0 É USADA PARA LER OS BITS NA MESMA ORDEM QUE ELES FORAM MONTADOS PELO COMPACTADOR
            bits += (((unsigned char)byte >> i) & 1) ? '1' : '0'; // >> i TRÁS O BIT i PARA A ÚLTIMA POSIÇÃO E DEPOIS APAGA OS OUTROS E ADICIONA '0' OU '1' DEPENDENDO DO RESULTADO
        }
    }
    return bits;
}

std::string nome_descompactado(const std::string& ari) { // COMO O COMPACTADOR ESVAZIA ARQUIVOS COM O MESMO NOME, CRIAMOS UM NOVO ARQUIVO COM NOME DIFERENTE PARA EVITAR PERDER O ARQUIVO ORIGINAL
    std::string original = ari;
    if (original.size() > 4 && original.substr(original.size() - 4) == ".ari") {
        original = original.substr(0, original.size() - 4);
    }
    std::size_t ponto = original.find_last_of('.');
    if (ponto == std::string::npos) return original + "_descompactado";
    return original.substr(0, ponto) + "_descompactado" + original.substr(ponto);
}

int main() { // NO MAIN CRIAMOS O TERMINAL PARA AS DECISÕES DO USUÁRIO E IMPLEMENTAMOS AS FUNÇÕES DE COMPACTAÇÃO DE DESCOMPACTAÇÃO
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
    #endif
    
    std::string opcao;
    std::cout << "[C] Compactar  ou  [D] Descompactar? ";
    std::getline(std::cin, opcao);

    if(opcao == "C" || opcao == "c") {
        std::string entrada;
        std::cout << "NOME DO ARQUIVO: ";
        std::getline(std::cin, entrada);

        std::string texto = ler_arquivo(entrada);
        if (texto.empty()) return 0;

        std::string tipo;
        std::cout << "MODELO [F] FIXO  ou  [A] ADAPTATIVO? ";
        std::getline(std::cin, tipo);
        char t = (tipo == "F" || tipo == "f") ? 'f' : 'a';

        Modelo* m = (t == 'f') ? (Modelo*) new ModeloFixo() : new ModeloAdaptativo();
        std::string bits = codificador(texto, m);
        delete m;

        std::string saida = entrada + ".ari";
        gravar_compactado(saida, texto.size(), t, bits);

        std::size_t tam_ari = 9 + (bits.size() + 7) / 8;
        std::cout << "GERADO: " << saida << std::endl;
        std::cout <<texto.size() << " bytes -> " << tam_ari << " bytes (" << 100.0 * tam_ari / texto.size() << "% do original)" << std::endl;
    }
    else if (opcao == "D" || opcao == "d") {
        std::string entrada;
        std::cout << "NOME DO ARQUIVO .ari: ";
        std::getline(std::cin, entrada);

        std::ifstream teste(entrada, std::ios::binary);
        if (!teste) {
            std::cout << "NÃO FOI POSSIVEL ABRIR: " << entrada << std::endl;
            return 0;
        }
        teste.close();

        uint64_t n;
        char modelo;
        std::string bits = ler_compactado(entrada, n, modelo);

        Modelo* m = (modelo == 'f') ? (Modelo*) new ModeloFixo() : new ModeloAdaptativo();
        std::string texto = decodificador(bits, n, m);
        delete m;

        std::string saida = nome_descompactado(entrada);
        gravar_arquivo(saida, texto);
        std::cout << "GERADO: " << saida << " (" << texto.size() << " bytes)" << std::endl;
    }
    else {
        std::cout << "OPCAO INVALIDA." << std::endl;
    }

    return 0;
}
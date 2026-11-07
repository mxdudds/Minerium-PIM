# Minerium

Este é o projeto do nosso grupo no Projeto Integrado Multidisciplinar (PIM): um sistema de rastreabilidade por lote de terras raras para uma mineradora fictícia.

Nós fizemos o programa em C, e ele guarda todos os dados em um banco SQL Server. O C conversa com o banco por meio do ODBC. Ele funciona somente no Windows 10 ou 11 de 64 bits.

## Como as peças se encaixam

Antes de instalar qualquer coisa, vale entender o papel de cada parte:

- **O programa em C (`minerium.exe`)** mostra as telas no terminal, pede os dados ao usuário e faz as validações simples.
- **O ODBC** é a ponte que deixa o C conversar com o SQL Server.
- **O SQL Server** é onde os dados ficam guardados de verdade. Por isso, se fechamos o programa, nada se perde.

## Arquivos do projeto

| Arquivo | O que é |
| --- | --- |
| `1_banco.sql` | Cria o banco `Minerium` e as 13 tabelas |
| `2_dados_e_logica.sql` | Cria índices, views e a procedure, e insere os dados de teste |
| `3_comandos_uteis.sql` | Comandos SQL para consultar, cadastrar, alterar e apagar dados |
| `minerium.c` | Código-fonte do programa |
| `conexao.exemplo.txt` | Modelo da string de conexão (sem senha) |

## Programas que precisam ser instalados

| Programa | Para que a gente usa | Precisa? |
| --- | --- | --- |
| SQL Server Express (2019 ou 2022) | É o servidor do banco de dados | Sim |
| SQL Server Management Studio (SSMS) | Serve para rodar os scripts `.sql` e conferir os dados | Sim |
| Microsoft ODBC Driver 17 ou 18 for SQL Server | Deixa o programa em C conversar com o SQL Server | Sim, se não aparecer na lista do `odbcad32` (passo 3) |
| GCC pelo MSYS2 (UCRT64) | É o compilador que transforma o `minerium.c` no `minerium.exe` | Sim, para compilar. Quem já tem o `.exe` não precisa |

Para baixar, pesquise o nome do programa no site oficial (Microsoft, MSYS2, Git). Usei só os sites oficiais, para não correr o risco de baixar algo errado.

## Instalação passo a passo

Eu sigo sempre esta ordem. Em cada passo tem um **Conferir**, para saber se deu certo antes de ir para o próximo.

### Passo 1: SQL Server Express

Primeiro precisamos do servidor onde o banco vai morar.

1. Baixe o **SQL Server Express** no site da Microsoft e execute o instalador.
2. Escolha o tipo de instalação **Básica** (Basic), aceite a licença e a pasta padrão, e clique em **Instalar**.
3. No final, a tela mostra o **nome da instância** (normalmente `SQLEXPRESS`). Anote esse nome, porque vamos usar mais adiante. Clique em **Fechar**.
4. **Conferir:** aperte Win + R, digite `services.msc` e procure **SQL Server (SQLEXPRESS)**. O status deve ser **Em execução**. Se estiver parado, clique com o botão direito e escolha **Iniciar**.

### Passo 2: SQL Server Management Studio (SSMS)

O SSMS é o programa onde a gente abre os arquivos `.sql`, executa e vê as tabelas.

1. Baixe o **SSMS** no site da Microsoft e instale com as opções padrão. Ele pode pedir para reiniciar o computador.
2. Abra o SSMS. Na janela de conexão, preencha:
   - **Server name:** `localhost\SQLEXPRESS` (ou o nome que você anotou no passo 1)
   - **Authentication:** Windows Authentication
   - Marque **Trust server certificate**
3. Clique em **Connect**.
4. **Conferir:** o servidor aparece no painel Object Explorer, à esquerda.

### Passo 3: driver ODBC

O driver é o "tradutor" que o programa em C usa para falar com o SQL Server. Primeiro eu vejo se ele já existe no computador.

1. Aperte Win + R, digite `odbcad32` e Enter.
2. Abra a aba **Drivers** e procure `ODBC Driver 17 for SQL Server` ou `ODBC Driver 18 for SQL Server`.
3. Se algum dos dois já aparece, anote o nome exato (vamos usar na conexão) e pule para o passo 4.
4. Se nenhum aparece, baixe o **Microsoft ODBC Driver for SQL Server** (versão 17 ou 18, 64 bits) no site da Microsoft e instale.
5. **Conferir:** abra o `odbcad32` de novo e veja o driver na lista.

### Passo 4: compilador GCC (MSYS2)

O programa está escrito em C, então precisa ser compilado. Para isso usamos o GCC, que vem no MSYS2.

1. Baixe o instalador no site **msys2.org** e execute, mantendo a pasta padrão `C:\msys64`.
2. No final, deixe marcada a opção de abrir o MSYS2. Se não abrir, procure no menu Iniciar por **MSYS2 UCRT64** (abra exatamente esse).
3. Na janela preta, rode o comando abaixo e confirme com Enter quando ele perguntar:

   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc
   ```

4. Agora colocamos o compilador no PATH do Windows. Isso é o que permite o Prompt de Comando encontrar o `gcc`:
   1. Aperte a tecla Windows e digite **variáveis de ambiente**.
   2. Abra **Editar as variáveis de ambiente do sistema** e clique em **Variáveis de Ambiente**.
   3. Em **Variáveis do usuário**, selecione **Path** e clique em **Editar**.
   4. Clique em **Novo**, cole `C:\msys64\ucrt64\bin` e confirme com **OK** em todas as janelas.
5. **Feche todos os Prompts de Comando e abra um novo.** Sem isso o Windows não enxerga o `gcc`.
6. **Conferir:**

   ```
   gcc --version
   ```

   Deve aparecer a versão do GCC.

### Passo 5: Git (só para o trabalho em grupo)

O Git guarda o histórico do código e deixa a gente trabalhar junto sem sobrescrever o arquivo um do outro.

1. No Prompt de Comando:

   ```
   winget install --id Git.Git -e
   ```

2. Abra um Prompt novo e configure seu nome e e-mail (o mesmo e-mail da conta do GitHub):

   ```
   git config --global user.name "Seu Nome"
   git config --global user.email "seuemail@exemplo.com"
   ```

3. **Conferir:** `git --version` mostra a versão.

## Criar o banco de dados

Com tudo instalado, vamos criar o banco. A ordem importa: primeiro o arquivo 1, depois o 2.

1. No SSMS, vá em **File > Open > File** e abra `1_banco.sql`.
2. Aperte **F5** com o arquivo inteiro (sem selecionar nada). No final aparece a lista de 13 tabelas.
3. Abra `2_dados_e_logica.sql` e aperte **F5**. Na aba **Messages**, os 7 testes devem terminar com a palavra "bloqueado". Isso quer dizer que o banco recusou os dados inválidos, como era para ser. Se o trecho final (o backup) der erro de permissão, pode ignorar, porque o programa não depende dele.
4. **Conferir**, em uma nova consulta:

   ```sql
   USE Minerium;
   SELECT COUNT(*) AS funcionarios FROM dbo.Funcionario;    -- 4
   SELECT COUNT(*) AS lotes        FROM dbo.Lote;           -- 3
   SELECT COUNT(*) AS historico    FROM dbo.Historico_Lote; -- 9
   ```

Se precisar refazer tudo do zero, apague o banco antes (veja a Parte 5 do `3_comandos_uteis.sql`).

## Configurar a conexão

O programa precisa saber o nome do servidor e do driver para achar o banco. Dá para fazer de duas formas, e basta escolher **uma**.

**Opção A: editar o código.** No topo do `minerium.c`, ajuste a `CONN_STR`. Atenção: no código C a barra é dupla (`\\`), porque a barra invertida é um caractere especial na linguagem C.

```c
#define CONN_STR \
    "Driver={ODBC Driver 17 for SQL Server};" \
    "Server=localhost\\SQLEXPRESS;" \
    "Database=Minerium;" \
    "Trusted_Connection=yes;" \
    "TrustServerCertificate=yes;"
```

**Opção B: arquivo `conexao.txt`** (não precisa recompilar). Copie o `conexao.exemplo.txt` para `conexao.txt`, na mesma pasta do `minerium.exe`, e ajuste a única linha dele. Aqui a barra é simples:

```
Driver={ODBC Driver 17 for SQL Server};Server=localhost\SQLEXPRESS;Database=Minerium;Trusted_Connection=yes;TrustServerCertificate=yes;
```

Ajustes comuns:
- **Driver 18:** troque `17` por `18` no nome do driver.
- **Outro servidor:** troque `localhost\SQLEXPRESS` pelo nome que aparece no SSMS (por exemplo `DESKTOP-ABC123\SQLEXPRESS`).
- **Login e senha do SQL Server** em vez do usuário do Windows: troque `Trusted_Connection=yes;` por `UID=sa;PWD=suasenha;`.

O `conexao.txt` não vai para o Git (está no `.gitignore`), porque pode ter senha dentro.

## Compilar e executar

No Prompt de Comando, dentro da pasta do projeto, rode os comandos abaixo. Eu uso um caminho simples, como `C:\PIM`, e evito pastas como Vídeos ou Desktop, porque o antivírus e o OneDrive podem bloquear o arquivo.

```
cd /d C:\PIM
gcc minerium.c -o minerium.exe -lodbc32 -Wall
minerium.exe
```

O que cada parte do comando faz:
- `-o minerium.exe` dá o nome ao programa gerado.
- `-lodbc32` liga a biblioteca ODBC do Windows. Sem ele o compilador reclama de `undefined reference`.
- `-Wall` mostra todos os avisos, o que ajuda a achar erros.

O arquivo `minerium.c` precisa estar salvo em UTF-8, para os acentos aparecerem certos.

## Usuários de teste

Os scripts já criam quatro usuários, um de cada perfil, para testar o que cada tipo de usuário pode fazer:

| Login | Senha | Perfil |
| --- | --- | --- |
| `ana.souza` | `Admin@123` | Administrador |
| `carlos.lima` | `Oper@123` | Operador |
| `marina.alves` | `Sup@123` | Supervisor |
| `paulo.rocha` | `Cons@123` | Consulta |

São dados fictícios, só para teste acadêmico.

## Problemas comuns

Estes foram os erros que a gente encontrou (ou que podem aparecer) e como resolver:

| Mensagem ou sintoma | Causa provável | O que fazer |
| --- | --- | --- |
| `'gcc' não é reconhecido` | Compilador fora do PATH, ou o Prompt foi aberto antes de ajustar o PATH | Refaça o passo 4 e abra um Prompt novo |
| `Permission denied` ao compilar | Antivírus ou programa ainda aberto | Rode `taskkill /f /im minerium.exe`, apague o `.exe` e compile de novo, de preferência em `C:\PIM` |
| `undefined reference to SQL...` | Faltou ligar a biblioteca ODBC | Compile com `-lodbc32` |
| `Data source name not found` | Nome do driver errado ou driver ausente | Confira a aba Drivers do `odbcad32` e ajuste a string de conexão |
| `Login failed for user` | O usuário do Windows não tem acesso ao SQL Server | Use `UID` e `PWD` na string de conexão |
| `Server does not exist` | Nome da instância errado ou serviço parado | Confira o nome no SSMS e inicie o serviço no `services.msc` |
| `Cannot open database Minerium` | Os scripts não foram executados | Rode `1_banco.sql` e depois `2_dados_e_logica.sql` |
| Acentos estranhos na tela | Console sem UTF-8 ou arquivo salvo em outra codificação | Use o Prompt de Comando ou o Windows Terminal e salve o `.c` em UTF-8 |

## Versionamento com Git

Depois de criar um repositório vazio no GitHub, o primeiro envio é assim (só se faz uma vez):

```
cd /d C:\PIM
git init
git add minerium.c 1_banco.sql 2_dados_e_logica.sql 3_comandos_uteis.sql conexao.exemplo.txt README.md .gitignore
git commit -m "Versão inicial: sistema Minerium em C com banco SQL Server"
git branch -M main
git remote add origin https://github.com/SEU_USUARIO/pim-minerium.git
git push -u origin main
```

A rotina a cada alteração é:

```
git pull
git add .
git commit -m "Descreva o que mudou"
git push
```

Rode sempre o `git pull` antes de começar a trabalhar, para pegar o que os colegas enviaram.

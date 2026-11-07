USE Minerium;   
GO

DROP TABLE dbo.Historico_lote;
GO
CREATE TABLE dbo.Historico_Lote (
    id_historico        INT IDENTITY(1,1) NOT NULL,
    id_lote             INT NOT NULL,
    id_funcionario      INT NOT NULL,
    etapa               VARCHAR(50)  NOT NULL,
    status              VARCHAR(30)  NOT NULL,
    local               VARCHAR(100) NOT NULL,
    descricao_atividade VARCHAR(MAX) NULL,
    qtd_movimentada     DECIMAL(12,2) NOT NULL CONSTRAINT DF_Hist_qtd_mov   DEFAULT 0,
    qtd_perda           DECIMAL(12,2) NOT NULL CONSTRAINT DF_Hist_qtd_perda DEFAULT 0,
    data_hora           DATETIME      NOT NULL CONSTRAINT DF_Hist_data_hora DEFAULT GETDATE(),
    CONSTRAINT PK_Historico_Lote   PRIMARY KEY (id_historico),
    CONSTRAINT FK_Hist_Lote        FOREIGN KEY (id_lote)        REFERENCES dbo.Lote (id_lote),
    CONSTRAINT FK_Hist_Funcionario FOREIGN KEY (id_funcionario) REFERENCES dbo.Funcionario (id_funcionario),
    CONSTRAINT CK_Hist_etapa  CHECK (etapa IN ('Extracao', 'Processamento', 'Armazenamento', 'Transporte', 'Entrega')),
    CONSTRAINT CK_Hist_status CHECK (status IN ('Extraido', 'Em processamento', 'Armazenado', 'Transportado', 'Entregue')),
    CONSTRAINT CK_Hist_qtds   CHECK (qtd_movimentada >= 0 AND qtd_perda >= 0)
);
GO

ALTER TABLE dbo.Produto ADD
    quantidade     DECIMAL(12,2) NOT NULL,
    unidade_medida VARCHAR(10)   NOT NULL CONSTRAINT DF_Produto_unidade DEFAULT 'kg';
GO
ALTER TABLE dbo.Produto ADD
    CONSTRAINT CK_Produto_quantidade CHECK (quantidade > 0),
    CONSTRAINT CK_Produto_unidade    CHECK (unidade_medida IN ('kg', 'g', 't', 'un'));
GO

ALTER TABLE dbo.Entrega ADD
    quantidade DECIMAL(12,2) NOT NULL;
GO
ALTER TABLE dbo.Entrega ADD
    CONSTRAINT CK_Entrega_quantidade CHECK (quantidade > 0);
GO

CREATE INDEX IX_Funcionario_perfil  ON dbo.Funcionario (id_perfil);
CREATE INDEX IX_Lote_mina           ON dbo.Lote (id_mina);
CREATE INDEX IX_Lote_mineral        ON dbo.Lote (id_mineral);
CREATE INDEX IX_Lote_funcionario    ON dbo.Lote (id_funcionario);
CREATE INDEX IX_Lote_status         ON dbo.Lote (status);
CREATE INDEX IX_Hist_lote_data      ON dbo.Historico_Lote (id_lote, data_hora);
CREATE INDEX IX_Hist_funcionario    ON dbo.Historico_Lote (id_funcionario);
CREATE INDEX IX_Produto_lote        ON dbo.Produto (id_lote);
CREATE INDEX IX_Pedido_cliente      ON dbo.Pedido_cliente (id_cliente);
CREATE INDEX IX_Entrega_pedido      ON dbo.Entrega (id_pedido_cliente);
CREATE INDEX IX_Entrega_produto     ON dbo.Entrega (id_produto);
CREATE INDEX IX_Entrega_funcionario ON dbo.Entrega (id_funcionario);
CREATE INDEX IX_Pagamento_pedido    ON dbo.Pagamento (id_pedido_cliente);
GO

CREATE UNIQUE INDEX UX_Pedido_nota_fiscal
    ON dbo.Pedido_cliente (numero_nota_fiscal)
    WHERE numero_nota_fiscal IS NOT NULL;
GO

CREATE OR ALTER VIEW dbo.vw_rastreabilidade_lote AS
SELECT
    l.codigo_lote, mi.nome AS mina_origem, mn.nome AS mineral,
    l.quantidade AS quantidade_atual, l.unidade_medida, l.status AS status_atual,
    l.data_extracao, fc.nome AS responsavel_cadastro,
    h.data_hora, h.etapa, h.status AS status_da_etapa, h.local,
    h.descricao_atividade, h.qtd_movimentada, h.qtd_perda, fh.nome AS responsavel_etapa
FROM dbo.Lote l
JOIN dbo.Mina        mi ON mi.id_mina        = l.id_mina
JOIN dbo.Mineral     mn ON mn.id_mineral     = l.id_mineral
JOIN dbo.Funcionario fc ON fc.id_funcionario = l.id_funcionario
LEFT JOIN dbo.Historico_Lote h  ON h.id_lote = l.id_lote
LEFT JOIN dbo.Funcionario    fh ON fh.id_funcionario = h.id_funcionario;
GO

CREATE OR ALTER PROCEDURE dbo.sp_registrar_etapa
    @codigo_lote         VARCHAR(20),
    @login               VARCHAR(50),
    @etapa               VARCHAR(50),
    @status              VARCHAR(30),
    @local               VARCHAR(100),
    @descricao_atividade VARCHAR(MAX)  = NULL,
    @qtd_movimentada     DECIMAL(12,2) = 0,
    @qtd_perda           DECIMAL(12,2) = 0
AS
BEGIN
    SET NOCOUNT ON;
    SET XACT_ABORT ON;

    DECLARE @id_lote INT, @id_func INT, @qtd_atual DECIMAL(12,2);

    SELECT @id_lote = id_lote, @qtd_atual = quantidade
      FROM dbo.Lote WHERE codigo_lote = @codigo_lote;

    SELECT @id_func = id_funcionario
      FROM dbo.Funcionario WHERE login = @login AND status = 'Ativo';

    IF @id_lote IS NULL THROW 50001, 'Lote nao encontrado.', 1;
    IF @id_func IS NULL THROW 50002, 'Funcionario nao encontrado ou inativo.', 1;
    IF @qtd_perda >= @qtd_atual THROW 50003, 'A perda nao pode ser maior ou igual a quantidade atual do lote.', 1;

    BEGIN TRANSACTION;

        INSERT INTO dbo.Historico_Lote
            (id_lote, id_funcionario, etapa, status, local, descricao_atividade, qtd_movimentada, qtd_perda)
        VALUES
            (@id_lote, @id_func, @etapa, @status, @local, @descricao_atividade, @qtd_movimentada, @qtd_perda);

        UPDATE dbo.Lote
           SET status     = @status,
               quantidade = quantidade - @qtd_perda
         WHERE id_lote = @id_lote;

    COMMIT TRANSACTION;
END
GO

INSERT INTO dbo.Perfil (nome, descricao) VALUES
 ('Administrador', 'Acesso total ao sistema'),
 ('Operador',      'Cadastra lotes e registra etapas'),
 ('Supervisor',    'Acompanha operações, entregas e relatórios'),
 ('Consulta',      'Somente consulta e relatórios');

INSERT INTO dbo.Permissao (nome, descricao) VALUES
 ('CADASTRAR_FUNCIONARIO',     'Cadastrar e editar funcionários'),
 ('CADASTRAR_LOTE',            'Cadastrar lotes'),
 ('REGISTRAR_ETAPA',           'Registrar etapas e movimentações do lote'),
 ('ATUALIZAR_STATUS',          'Atualizar a situação do lote'),
 ('CONSULTAR_RASTREABILIDADE', 'Consultar o histórico de um lote'),
 ('CADASTRAR_CLIENTE',         'Cadastrar clientes industriais'),
 ('REGISTRAR_ENTREGA',         'Registrar entregas'),
 ('GERAR_RELATORIO',           'Gerar relatórios');
GO

INSERT INTO dbo.Perfil_Permissao (id_perfil, id_permissao)
SELECT p.id_perfil, pm.id_permissao
  FROM dbo.Perfil p CROSS JOIN dbo.Permissao pm
 WHERE p.nome = 'Administrador';

INSERT INTO dbo.Perfil_Permissao (id_perfil, id_permissao)
SELECT p.id_perfil, pm.id_permissao
  FROM dbo.Perfil p JOIN dbo.Permissao pm
    ON pm.nome IN ('CADASTRAR_LOTE','REGISTRAR_ETAPA','ATUALIZAR_STATUS','CONSULTAR_RASTREABILIDADE')
 WHERE p.nome = 'Operador';

INSERT INTO dbo.Perfil_Permissao (id_perfil, id_permissao)
SELECT p.id_perfil, pm.id_permissao
  FROM dbo.Perfil p JOIN dbo.Permissao pm
    ON pm.nome IN ('CADASTRAR_LOTE','REGISTRAR_ETAPA','ATUALIZAR_STATUS','CONSULTAR_RASTREABILIDADE',
                   'CADASTRAR_CLIENTE','REGISTRAR_ENTREGA','GERAR_RELATORIO')
 WHERE p.nome = 'Supervisor';

INSERT INTO dbo.Perfil_Permissao (id_perfil, id_permissao)
SELECT p.id_perfil, pm.id_permissao
  FROM dbo.Perfil p JOIN dbo.Permissao pm
    ON pm.nome IN ('CONSULTAR_RASTREABILIDADE','GERAR_RELATORIO')
 WHERE p.nome = 'Consulta';
GO

INSERT INTO dbo.Funcionario (id_perfil, nome, login, senha_hash) VALUES
 ((SELECT id_perfil FROM dbo.Perfil WHERE nome = 'Administrador'), 'Ana Souza',    'ana.souza',    CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', 'Admin@123'), 2)),
 ((SELECT id_perfil FROM dbo.Perfil WHERE nome = 'Operador'),      'Carlos Lima',  'carlos.lima',  CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', 'Oper@123'),  2)),
 ((SELECT id_perfil FROM dbo.Perfil WHERE nome = 'Supervisor'),    'Marina Alves', 'marina.alves', CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', 'Sup@123'),   2)),
 ((SELECT id_perfil FROM dbo.Perfil WHERE nome = 'Consulta'),      'Paulo Rocha',  'paulo.rocha',  CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', 'Cons@123'),  2));
GO

INSERT INTO dbo.Mina (nome, localizacao) VALUES
 ('Mina Serra Azul', 'Complexo Serra Azul, Minas Gerais');

INSERT INTO dbo.Mineral (nome, descricao) VALUES
 ('Neodímio', 'Terra rara usada em ímãs permanentes de alto desempenho'),
 ('Lantânio', 'Terra rara usada em catalisadores e baterias'),
 ('Cério',    'Terra rara usada em polimento e catalisadores');
GO

INSERT INTO dbo.Lote (id_mina, id_mineral, id_funcionario, quantidade, unidade_medida, data_extracao) VALUES
 ((SELECT id_mina FROM dbo.Mina WHERE nome = 'Mina Serra Azul'), (SELECT id_mineral FROM dbo.Mineral WHERE nome = 'Neodímio'),
  (SELECT id_funcionario FROM dbo.Funcionario WHERE login = 'carlos.lima'), 1500.00, 'kg', '2026-09-10'),
 ((SELECT id_mina FROM dbo.Mina WHERE nome = 'Mina Serra Azul'), (SELECT id_mineral FROM dbo.Mineral WHERE nome = 'Cério'),
  (SELECT id_funcionario FROM dbo.Funcionario WHERE login = 'carlos.lima'),  800.00, 'kg', '2026-09-18'),
 ((SELECT id_mina FROM dbo.Mina WHERE nome = 'Mina Serra Azul'), (SELECT id_mineral FROM dbo.Mineral WHERE nome = 'Lantânio'),
  (SELECT id_funcionario FROM dbo.Funcionario WHERE login = 'carlos.lima'),    2.50, 't',  '2026-10-02');
GO

EXEC dbo.sp_registrar_etapa 'LT-000001','carlos.lima','Extracao','Extraido','Frente de lavra A','Extração do minério bruto',1500,0;
EXEC dbo.sp_registrar_etapa 'LT-000001','carlos.lima','Processamento','Em processamento','Usina de beneficiamento','Concentração e separação magnética',1500,45;
EXEC dbo.sp_registrar_etapa 'LT-000001','marina.alves','Armazenamento','Armazenado','Galpão 2','Estocagem do concentrado',1455,0;
EXEC dbo.sp_registrar_etapa 'LT-000001','marina.alves','Transporte','Transportado','Expedição','Carregamento e transporte ao cliente',1455,0;
EXEC dbo.sp_registrar_etapa 'LT-000001','marina.alves','Entrega','Entregue','Fábrica do cliente','Entrega concluída e conferida',1000,0;

EXEC dbo.sp_registrar_etapa 'LT-000002','carlos.lima','Extracao','Extraido','Frente de lavra B','Extração do minério bruto',800,0;
EXEC dbo.sp_registrar_etapa 'LT-000002','carlos.lima','Processamento','Em processamento','Usina de beneficiamento','Lixiviação e purificação',800,20;
EXEC dbo.sp_registrar_etapa 'LT-000002','marina.alves','Transporte','Transportado','Expedição','Transporte ao cliente',780,0;

EXEC dbo.sp_registrar_etapa 'LT-000003','carlos.lima','Extracao','Extraido','Frente de lavra A','Extração do minério bruto',2.5,0;
GO

INSERT INTO dbo.Produto (id_lote, nome, descricao, quantidade, unidade_medida) VALUES
 ((SELECT id_lote FROM dbo.Lote WHERE codigo_lote = 'LT-000001'), 'Óxido de Neodímio', 'Pureza 99%', 1000.00, 'kg'),
 ((SELECT id_lote FROM dbo.Lote WHERE codigo_lote = 'LT-000002'), 'Óxido de Cério',    'Pureza 99%',  500.00, 'kg');

INSERT INTO dbo.Cliente (razao_social, cnpj, contato, email, senha_hash) VALUES
 ('Magnética Industrial Ltda', '11111111000111', 'Roberto Dias',   'compras@magnetica.example.com',
   CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', 'Cli@123'), 2)),
 ('TechMotores S.A.',          '22222222000122', 'Fernanda Costa', 'suprimentos@techmotores.example.com',
   CONVERT(VARCHAR(255), HASHBYTES('SHA2_256', 'Cli@456'), 2));
GO

INSERT INTO dbo.Pedido_cliente (id_cliente, data_pedido, status_pedido, valor_total, numero_nota_fiscal) VALUES
 ((SELECT id_cliente FROM dbo.Cliente WHERE cnpj = '11111111000111'), '2026-10-05', 'Entregue',   185000.00, 'NF-000123'),
 ((SELECT id_cliente FROM dbo.Cliente WHERE cnpj = '22222222000122'), '2026-10-12', 'A Caminho',   98000.00, 'NF-000124');

INSERT INTO dbo.Entrega (id_pedido_cliente, id_produto, id_funcionario, quantidade, data_entrega, status) VALUES
 ((SELECT id_pedido_cliente FROM dbo.Pedido_cliente WHERE numero_nota_fiscal = 'NF-000123'),
  (SELECT id_produto FROM dbo.Produto WHERE nome = 'Óxido de Neodímio'),
  (SELECT id_funcionario FROM dbo.Funcionario WHERE login = 'marina.alves'), 1000.00, '2026-10-20', 'Entregue'),
 ((SELECT id_pedido_cliente FROM dbo.Pedido_cliente WHERE numero_nota_fiscal = 'NF-000124'),
  (SELECT id_produto FROM dbo.Produto WHERE nome = 'Óxido de Cério'),
  (SELECT id_funcionario FROM dbo.Funcionario WHERE login = 'marina.alves'),  500.00, NULL, 'Em Transporte');

INSERT INTO dbo.Pagamento (id_pedido_cliente, valor_pago, data_pagamento, metodo_pagamento, status) VALUES
 ((SELECT id_pedido_cliente FROM dbo.Pedido_cliente WHERE numero_nota_fiscal = 'NF-000123'), 185000.00, '2026-10-22', 'PIX', 'Pago');
GO


CREATE OR ALTER VIEW dbo.vw_origem_materiais AS
SELECT
    mi.nome AS mina,
    mn.nome AS mineral,
    l.unidade_medida,
    COUNT(*)          AS qtd_lotes,
    SUM(l.quantidade) AS quantidade_total
FROM dbo.Lote l
JOIN dbo.Mina    mi ON mi.id_mina    = l.id_mina
JOIN dbo.Mineral mn ON mn.id_mineral = l.id_mineral
GROUP BY mi.nome, mn.nome, l.unidade_medida;
GO

CREATE OR ALTER VIEW dbo.vw_entregas_realizadas AS
SELECT
    c.razao_social AS cliente,
    p.id_pedido_cliente,
    p.numero_nota_fiscal,
    e.id_entrega,
    e.data_entrega,
    e.status       AS status_entrega,
    pr.nome        AS produto,
    e.quantidade   AS quantidade_entregue,
    pr.unidade_medida,
    l.codigo_lote  AS lote_origem,
    mi.nome        AS mina_origem,
    f.nome         AS responsavel_entrega
FROM dbo.Entrega e
JOIN dbo.Pedido_cliente p ON p.id_pedido_cliente = e.id_pedido_cliente
JOIN dbo.Cliente        c ON c.id_cliente        = p.id_cliente
JOIN dbo.Produto       pr ON pr.id_produto       = e.id_produto
JOIN dbo.Lote           l ON l.id_lote           = pr.id_lote
JOIN dbo.Mina          mi ON mi.id_mina          = l.id_mina
JOIN dbo.Funcionario    f ON f.id_funcionario    = e.id_funcionario;
GO

-- Conferências (devem mostrar dados)
SELECT codigo_lote, status, quantidade, unidade_medida FROM dbo.Lote;
SELECT codigo_lote, mineral, etapa, status_da_etapa, local, qtd_perda
  FROM dbo.vw_rastreabilidade_lote ORDER BY codigo_lote, data_hora;
SELECT * FROM dbo.vw_origem_materiais;
SELECT * FROM dbo.vw_entregas_realizadas ORDER BY id_entrega;
SELECT p.nome AS perfil, COUNT(*) AS qtd_permissoes
  FROM dbo.Perfil_Permissao pp JOIN dbo.Perfil p ON p.id_perfil = pp.id_perfil
 GROUP BY p.nome;
SELECT id_funcionario, nome, login, status, senha_hash FROM dbo.Funcionario;
GO

-- Testes de integridade (cada um deve imprimir "bloqueado")
BEGIN TRY
    UPDATE dbo.Cliente SET cnpj = '22222222000122' WHERE cnpj = '11111111000111';
    PRINT 'TESTE 1: ERRO - o banco aceitou CNPJ duplicado!';
END TRY
BEGIN CATCH
    PRINT 'TESTE 1 (CNPJ duplicado) bloqueado: ' + ERROR_MESSAGE();
END CATCH

BEGIN TRY
    UPDATE dbo.Funcionario SET login = 'carlos.lima' WHERE login = 'ana.souza';
    PRINT 'TESTE 2: ERRO - o banco aceitou login duplicado!';
END TRY
BEGIN CATCH
    PRINT 'TESTE 2 (login duplicado) bloqueado: ' + ERROR_MESSAGE();
END CATCH

BEGIN TRY
    UPDATE dbo.Pedido_cliente SET numero_nota_fiscal = 'NF-000123' WHERE numero_nota_fiscal = 'NF-000124';
    PRINT 'TESTE 3: ERRO - o banco aceitou nota fiscal duplicada!';
END TRY
BEGIN CATCH
    PRINT 'TESTE 3 (nota fiscal duplicada) bloqueado: ' + ERROR_MESSAGE();
END CATCH

BEGIN TRY
    UPDATE dbo.Lote SET status = 'Perdido' WHERE codigo_lote = 'LT-000001';
    PRINT 'TESTE 4: ERRO - o banco aceitou um status invalido!';
END TRY
BEGIN CATCH
    PRINT 'TESTE 4 (status invalido) bloqueado: ' + ERROR_MESSAGE();
END CATCH

BEGIN TRY
    UPDATE dbo.Lote SET quantidade = -5 WHERE codigo_lote = 'LT-000003';
    PRINT 'TESTE 5: ERRO - o banco aceitou quantidade negativa!';
END TRY
BEGIN CATCH
    PRINT 'TESTE 5 (quantidade negativa) bloqueado: ' + ERROR_MESSAGE();
END CATCH

BEGIN TRY
    DELETE FROM dbo.Mina WHERE nome = 'Mina Serra Azul';
    PRINT 'TESTE 6: ERRO - o banco apagou uma mina que tem lotes!';
END TRY
BEGIN CATCH
    PRINT 'TESTE 6 (apagar mina com lotes) bloqueado: ' + ERROR_MESSAGE();
END CATCH

BEGIN TRY
    EXEC dbo.sp_registrar_etapa 'LT-000003','carlos.lima','Processamento','Em processamento','Usina','Teste',1,999;
    PRINT 'TESTE 7: ERRO - a procedure aceitou perda maior que o lote!';
END TRY
BEGIN CATCH
    PRINT 'TESTE 7 (perda maior que o lote) bloqueado: ' + ERROR_MESSAGE();
END CATCH
GO

/* ------------------------------------------------------------
   BACKUP (opcional). Se der erro de permissão na pasta de backup,
   pode ignorar: o programa em C funciona sem isso.
   ------------------------------------------------------------ */
USE master;
GO
DECLARE @pasta   NVARCHAR(260) = CAST(SERVERPROPERTY('InstanceDefaultBackupPath') AS NVARCHAR(260));
IF RIGHT(@pasta, 1) <> N'\' SET @pasta = @pasta + N'\';
DECLARE @arquivo NVARCHAR(400) = @pasta + N'Minerium_completo.bak';

BACKUP DATABASE Minerium
TO DISK = @arquivo
WITH FORMAT, INIT, NAME = N'Minerium - backup completo', CHECKSUM, STATS = 10;

RESTORE VERIFYONLY FROM DISK = @arquivo WITH CHECKSUM;

PRINT 'Backup gravado em: ' + @arquivo;
GO

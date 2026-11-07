CREATE DATABASE Minerium;
GO

USE Minerium;  
GO

CREATE TABLE dbo.Perfil (
    id_perfil  INT IDENTITY(1,1) NOT NULL,
    nome       VARCHAR(50)  NOT NULL,
    descricao  VARCHAR(MAX) NULL,
    CONSTRAINT PK_Perfil      PRIMARY KEY (id_perfil),
    CONSTRAINT UQ_Perfil_nome UNIQUE (nome)
);
GO

CREATE TABLE dbo.Permissao (
    id_permissao INT IDENTITY(1,1) NOT NULL,
    nome         VARCHAR(50)  NOT NULL,
    descricao    VARCHAR(MAX) NULL,
    CONSTRAINT PK_Permissao      PRIMARY KEY (id_permissao),
    CONSTRAINT UQ_Permissao_nome UNIQUE (nome)
);
GO

CREATE TABLE dbo.Perfil_Permissao (
    id_perfil    INT NOT NULL,
    id_permissao INT NOT NULL,
    CONSTRAINT PK_Perfil_Permissao PRIMARY KEY (id_perfil, id_permissao),
    CONSTRAINT FK_PP_Perfil    FOREIGN KEY (id_perfil)    REFERENCES dbo.Perfil (id_perfil),
    CONSTRAINT FK_PP_Permissao FOREIGN KEY (id_permissao) REFERENCES dbo.Permissao (id_permissao)
);
GO

CREATE TABLE dbo.Funcionario (
    id_funcionario INT IDENTITY(1,1) NOT NULL,
    id_perfil      INT          NOT NULL,
    nome           VARCHAR(100) NOT NULL,
    login          VARCHAR(50)  NOT NULL,
    senha_hash     VARCHAR(255) NOT NULL,
    status         VARCHAR(20)  NOT NULL CONSTRAINT DF_Funcionario_status DEFAULT 'Ativo',
    CONSTRAINT PK_Funcionario        PRIMARY KEY (id_funcionario),
    CONSTRAINT UQ_Funcionario_login  UNIQUE (login),
    CONSTRAINT FK_Funcionario_Perfil FOREIGN KEY (id_perfil) REFERENCES dbo.Perfil (id_perfil),
    CONSTRAINT CK_Funcionario_status CHECK (status IN ('Ativo', 'Inativo'))
);
GO

CREATE TABLE dbo.Mina (
    id_mina     INT IDENTITY(1,1) NOT NULL,
    nome        VARCHAR(100) NOT NULL,
    localizacao VARCHAR(150) NULL,
    status      VARCHAR(20)  NOT NULL CONSTRAINT DF_Mina_status DEFAULT 'Ativa',
    CONSTRAINT PK_Mina        PRIMARY KEY (id_mina),
    CONSTRAINT UQ_Mina_nome   UNIQUE (nome),
    CONSTRAINT CK_Mina_status CHECK (status IN ('Ativa', 'Inativa'))
);
GO

CREATE TABLE dbo.Mineral (
    id_mineral INT IDENTITY(1,1) NOT NULL,
    nome       VARCHAR(100) NOT NULL,
    descricao  VARCHAR(MAX) NULL,
    CONSTRAINT PK_Mineral      PRIMARY KEY (id_mineral),
    CONSTRAINT UQ_Mineral_nome UNIQUE (nome)
);
GO

CREATE TABLE dbo.Lote (
    id_lote        INT IDENTITY(1,1) NOT NULL,
    id_mina        INT NOT NULL,
    id_mineral     INT NOT NULL,
    id_funcionario INT NOT NULL,
    codigo_lote    AS ('LT-' + RIGHT('000000' + CAST(id_lote AS VARCHAR(10)), 6)) PERSISTED NOT NULL,
    quantidade     DECIMAL(12,2) NOT NULL,
    unidade_medida VARCHAR(10)   NOT NULL CONSTRAINT DF_Lote_unidade DEFAULT 'kg',
    status         VARCHAR(30)   NOT NULL CONSTRAINT DF_Lote_status  DEFAULT 'Extraido',
    data_extracao  DATE          NOT NULL,
    data_cadastro  DATETIME      NOT NULL CONSTRAINT DF_Lote_data_cadastro DEFAULT GETDATE(),
    CONSTRAINT PK_Lote             PRIMARY KEY (id_lote),
    CONSTRAINT UQ_Lote_codigo      UNIQUE (codigo_lote),
    CONSTRAINT FK_Lote_Mina        FOREIGN KEY (id_mina)        REFERENCES dbo.Mina (id_mina),
    CONSTRAINT FK_Lote_Mineral     FOREIGN KEY (id_mineral)     REFERENCES dbo.Mineral (id_mineral),
    CONSTRAINT FK_Lote_Funcionario FOREIGN KEY (id_funcionario) REFERENCES dbo.Funcionario (id_funcionario),
    CONSTRAINT CK_Lote_quantidade  CHECK (quantidade > 0),
    CONSTRAINT CK_Lote_unidade     CHECK (unidade_medida IN ('kg', 'g', 't', 'un')),
    CONSTRAINT CK_Lote_status      CHECK (status IN ('Extraido', 'Em processamento', 'Armazenado', 'Transportado', 'Entregue'))
);
GO

CREATE TABLE dbo.Historico_lote (
    id_historico_lote   INT IDENTITY (1,1)  NOT NULL,
    id_lote             INT                 NOT NULL,
    id_funcionario      INT                 NOT NULL,
    status              VARCHAR(30)         NOT NULL,
    data_hora_registro  DATETIME            NOT NULL CONSTRAINT DF_Historico_lote_data DEFAULT GETDATE(),
    observacao          VARCHAR(MAX)        NULL,
    CONSTRAINT PK_Historico_lote             PRIMARY KEY (id_historico_lote),
    CONSTRAINT FK_Historico_lote_Lote        FOREIGN KEY (id_lote)        REFERENCES dbo.Lote (id_lote),
    CONSTRAINT FK_Historico_lote_funcionario FOREIGN KEY (id_funcionario) REFERENCES dbo.Funcionario (id_funcionario),
    CONSTRAINT CK_Historico_lote_status      CHECK (status IN ('Extraido', 'Em processamento', 'Armazenado', 'Transportado', 'Entregue'))
);
GO

CREATE TABLE dbo.Produto(
    id_produto  INT IDENTITY(1,1)   NOT NULL,
    id_lote     INT                 NOT NULL,
    nome        VARCHAR(100)        NOT NULL,
    descricao   VARCHAR(MAX)        NULL,
    CONSTRAINT PK_Produto       PRIMARY KEY (id_produto),
    CONSTRAINT FK_Produto_Lote  FOREIGN KEY (id_lote) REFERENCES dbo.Lote (id_lote)
);
GO

CREATE TABLE dbo.Cliente(
    id_cliente   INT IDENTITY(1,1)  NOT NULL,
    razao_social VARCHAR(150)       NOT NULL,
    cnpj         CHAR(14)           NOT NULL,
    contato      VARCHAR(100)       NOT NULL,
    email        VARCHAR(120)       NOT NULL,
    senha_hash   VARCHAR(255)       NOT NULL,
    status       VARCHAR(20)        NOT NULL CONSTRAINT DF_Cliente_status DEFAULT 'Ativo',
    CONSTRAINT PK_Cliente        PRIMARY KEY (id_cliente),
    CONSTRAINT UQ_Cliente_cnpj   UNIQUE (cnpj),
    CONSTRAINT UQ_Cliente_email  UNIQUE (email),
    CONSTRAINT CK_Cliente_status CHECK (status IN ('Ativo', 'Inativo'))
);
GO

CREATE TABLE dbo.Pedido_cliente(
    id_pedido_cliente  INT IDENTITY(1,1) NOT NULL,
    id_cliente         INT               NOT NULL,
    data_pedido        DATE              NOT NULL CONSTRAINT DF_Pedido_cliente_data DEFAULT CAST(GETDATE() AS DATE),
    status_pedido      VARCHAR(30)       NOT NULL,
    valor_total        DECIMAL(12,2)     NOT NULL,
    numero_nota_fiscal VARCHAR(20)       NULL,
    observacao         VARCHAR(MAX)      NULL,
    CONSTRAINT PK_Pedido_cliente         PRIMARY KEY (id_pedido_cliente),
    CONSTRAINT FK_Pedido_cliente_Cliente FOREIGN KEY (id_cliente) REFERENCES dbo.Cliente (id_cliente),
    CONSTRAINT CK_Pedido_valor           CHECK (valor_total > 0),
    CONSTRAINT CK_Pedido_status          CHECK (status_pedido IN ('A Pagar', 'Preparando', 'A Caminho', 'Entregue'))
);
GO

CREATE TABLE dbo.Entrega(
    id_entrega        INT IDENTITY(1,1) NOT NULL,
    id_pedido_cliente INT               NOT NULL,
    id_produto        INT               NOT NULL,
    id_funcionario    INT               NOT NULL,
    data_entrega      DATE              NULL,
    status            VARCHAR(20)       NOT NULL CONSTRAINT DF_Entrega_status DEFAULT 'Pendente',
    CONSTRAINT PK_Entrega                PRIMARY KEY (id_entrega),
    CONSTRAINT FK_Entrega_Pedido_cliente FOREIGN KEY (id_pedido_cliente) REFERENCES dbo.Pedido_cliente (id_pedido_cliente),
    CONSTRAINT FK_Entrega_Produto        FOREIGN KEY (id_produto)        REFERENCES dbo.Produto (id_produto),
    CONSTRAINT FK_Entrega_Funcionario    FOREIGN KEY (id_funcionario)    REFERENCES dbo.Funcionario (id_funcionario),
    CONSTRAINT CK_Entrega_status         CHECK (status IN ('Pendente', 'Em Transporte', 'Entregue', 'Cancelada'))
);
GO

CREATE TABLE dbo.Pagamento(
    id_pagamento      INT IDENTITY (1,1) NOT NULL,
    id_pedido_cliente INT                NOT NULL,
    valor_pago        DECIMAL(12,2)      NOT NULL,
    data_pagamento    DATE               NULL,
    metodo_pagamento  VARCHAR(30)        NOT NULL,
    status            VARCHAR(20)        NOT NULL CONSTRAINT DF_Pagamento_status DEFAULT 'Pendente',
    observacao        VARCHAR(MAX)       NULL,
    CONSTRAINT PK_Pagamento              PRIMARY KEY (id_pagamento),
    CONSTRAINT FK_Pagamento_Pedido_cliente FOREIGN KEY (id_pedido_cliente) REFERENCES dbo.Pedido_cliente (id_pedido_cliente),
    CONSTRAINT CK_Pagamento_valor_pago   CHECK (valor_pago > 0),
    CONSTRAINT CK_Pagamento_status       CHECK (status IN ('Pendente', 'Pago', 'Cancelado', 'Estornado'))
);
GO

-- deve listar 13 tabelas
SELECT name FROM sys.tables ORDER BY name;
GO

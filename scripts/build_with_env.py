#!/usr/bin/env python3
"""
Script simple para compilar y subir el firmware con variables del .env
Uso: python scripts/build_with_env.py [upload]
"""
import subprocess
import sys
import os
from pathlib import Path

# Cambiar al directorio del proyecto
project_dir = Path(__file__).parent.parent
os.chdir(project_dir)

# Cargar variables del .env
env_file = project_dir / '.env'
if env_file.exists():
    # Leer y exportar variables
    with open(env_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()
                # Remover comillas si están presentes
                if (value.startswith('"') and value.endswith('"')) or (value.startswith("'") and value.endswith("'")):
                    value = value[1:-1]
                # Exportar al entorno
                os.environ[key] = value

# Ejecutar pio
if len(sys.argv) > 1 and sys.argv[1] == 'upload':
    subprocess.run(['pio', 'run', '-t', 'upload'], check=True)
else:
    subprocess.run(['pio', 'run'], check=True)

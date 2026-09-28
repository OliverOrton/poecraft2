import io
import json
import pytest
from poecraft_economy.checkpoint import validate_configuration, restore, publish_snapshots, create_manifest
from .test_checkpoint import CheckpointManifestTests

class ServiceError(Exception):
    def __init__(self, code):
        self.response = {'Error': {'Code': code}}

class Store:
    def __init__(self, objects=None, error=None):
        self.objects = objects or {}
        self.error = error
        self.puts = []
    def get_object(self, *, Bucket, Key):
        if self.error: raise ServiceError(self.error)
        if Key not in self.objects: raise ServiceError('NoSuchKey')
        return {'Body': io.BytesIO(self.objects[Key])}
    def put_object(self, **args):
        assert args['IfNoneMatch'] == '*'
        self.puts.append(args['Key'])
        self.objects[args['Key']] = args['Body']

def test_config_fails_without_exposing_values():
    with pytest.raises(ValueError, match='missing refresh configuration') as error:
        validate_configuration({'AWS_SECRET_ACCESS_KEY': 'never-print-this'})
    assert 'never-print-this' not in str(error.value)

@pytest.mark.parametrize('code', ['AccessDenied', 'InvalidAccessKeyId', 'RequestTimeout', 'NoSuchBucket', '404', 'InvalidEndpoint'])
def test_restore_errors_never_bootstrap(tmp_path, code):
    with pytest.raises(ServiceError):
        restore(Store(error=code), 'bucket', tmp_path/'db', tmp_path/'manifest', bootstrap=True)
    assert not (tmp_path/'db').exists()

def test_only_explicit_missing_key_bootstraps(tmp_path):
    with pytest.raises(ValueError, match='explicit manual bootstrap'):
        restore(Store(), 'bucket', tmp_path/'db', tmp_path/'manifest')
    assert restore(Store(), 'bucket', tmp_path/'db', tmp_path/'manifest', bootstrap=True) is False

def test_corrupt_checkpoint_preserves_existing_history(tmp_path):
    database = tmp_path/'db'
    CheckpointManifestTests._create_database(database, 2)
    original = database.read_bytes()
    manifest = create_manifest(database)
    store = Store({'database/latest.json': json.dumps(manifest).encode(), manifest['database_key']: b'corrupt'})
    with pytest.raises(ValueError, match='size mismatch'):
        restore(store, 'bucket', database, tmp_path/'manifest', bootstrap=True)
    assert database.read_bytes() == original


def test_verified_checkpoint_restores_without_bootstrap(tmp_path):
    source = tmp_path/'source.db'
    CheckpointManifestTests._create_database(source, 2)
    manifest = create_manifest(source)
    store = Store({'database/latest.json': json.dumps(manifest).encode(), manifest['database_key']: source.read_bytes()})
    assert restore(store, 'bucket', tmp_path/'restored.db', tmp_path/'manifest.json') is True
    assert (tmp_path/'restored.db').read_bytes() == source.read_bytes()

def test_remote_snapshot_create_verify_and_collision(tmp_path):
    name = 'a'*64 + '.json'
    path = tmp_path/name
    path.write_bytes(b'original\r\n')
    store = Store()
    publish_snapshots(store, 'bucket', tmp_path)
    publish_snapshots(store, 'bucket', tmp_path)
    assert len(store.puts) == 1
    path.write_bytes(b'original\n')
    with pytest.raises(ValueError, match='collision'):
        publish_snapshots(store, 'bucket', tmp_path)
    assert store.objects['snapshots/'+name] == b'original\r\n'

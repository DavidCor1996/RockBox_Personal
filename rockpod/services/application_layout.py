"""The stable application order shared by RockPod and iPodJS firmware."""
from pathlib import Path

from services.file_safety import atomic_write_text

APPLICATIONS = (
    ('clock', 'Clock'), ('desktop-mode', 'Desktop Mode'),
    ('minivmac', 'Mini vMac'), ('youtube', 'YouTube'), ('twitch', 'Twitch'),
    ('spotify-wrapped', 'Spotify Wrapped'), ('tiktok', 'TikTok'),
    ('onlyfans', 'OnlyFans'), ('instagram', 'Instagram'), ('reddit', 'Reddit'),
    ('twitter', 'Twitter'), ('msn', 'MSN Messenger'),
    ('achievements', 'Achievements'), ('directv', 'DIRECTV'),
    ('sitekick', 'Sitekick'), ('tamagotchi', 'Tamagotchi'), ('maps', 'Maps'),
    ('weather', 'Weather'), ('calm', 'Calm'), ('internet', 'Internet'),
    ('pocket-sky', 'Pocket Sky'), ('qr-codes', 'QR Codes'),
    ('pokedex', 'Pokedex'), ('netflix', 'Netflix'),
    ('rolo-launcher', 'RoloLauncher'), ('rockbox', 'Rockbox'),
)
ORDER_PATH = Path('.rockbox/ipodjs/applications-order.txt')


def normalize_order(ids):
    known = [key for key, _ in APPLICATIONS]
    result = []
    for key in ids:
        if key in known and key not in result:
            result.append(key)
    return result + [key for key in known if key not in result]


class ApplicationLayoutService:
    def __init__(self, repo_root):
        self.repo_root = Path(repo_root)

    def _root(self, mount):
        if not mount:
            raise ValueError('Connect an iPod Classic or Video first.')
        root = Path(mount)
        if not root.is_dir() or not (root / '.rockbox').is_dir():
            raise ValueError('The iPod is no longer connected.')
        for path in [root, root / '.rockbox', root / '.rockbox/ipodjs',
                     root / ORDER_PATH]:
            if path.is_symlink():
                raise ValueError('The application layout path cannot be a symbolic link.')
        info = (root / '.rockbox/rockbox-info.txt').read_text()
        target = next((line.split(':', 1)[1].strip() for line in info.splitlines()
                       if line.startswith('Target:')), '')
        if target not in {'ipod6g', 'ipodvideo'}:
            raise ValueError('Application layouts require an iPod Classic or Video.')
        return root, target

    def load(self, mount):
        root, _ = self._root(mount)
        path = root / ORDER_PATH
        ids = path.read_text().splitlines()[:64] if path.exists() else []
        return normalize_order(ids)

    def visible_ids(self, mount):
        root, target = self._root(mount)
        hidden = set()
        if (root / '.rockbox/onlyfans/hidden').exists():
            hidden.add('onlyfans')
        if target != 'ipod6g':
            hidden.update({'desktop-mode', 'rolo-launcher', 'rockbox'})
        return [key for key, _ in APPLICATIONS if key not in hidden]

    def save(self, mount, visible_order):
        root, _ = self._root(mount)
        visible = self.visible_ids(mount)
        if len(visible_order) != len(set(visible_order)) or set(visible_order) != set(visible):
            raise ValueError('The applications changed. Reload the layout before saving.')
        # Keep hidden/unsupported entries in their saved positions, so changing
        # device visibility never loses the user's earlier arrangement.
        iterator = iter(visible_order)
        order = [next(iterator) if key in visible else key for key in self.load(mount)]
        text = '\n'.join(order) + '\n'
        atomic_write_text(root / ORDER_PATH, text)
        if (root / ORDER_PATH).read_text() != text:
            raise OSError('The iPod did not retain the saved application order.')
        return order

    def icon_path(self, key):
        paths = [
            self.repo_root / 'resources/tv-applications' / f'{key}.80x80x24.bmp',
            self.repo_root / 'assets/ipodjs/rockbox/tv-applications' / f'{key}.80x80x24.bmp',
            self.repo_root / 'assets/ipodjs/rockbox/applications' / f'{key}.46x46x24.bmp',
        ]
        return next((path for path in paths if path.is_file()), paths[0])
